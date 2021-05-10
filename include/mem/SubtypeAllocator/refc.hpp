#pragma once

#include <atomic>
#include <cassert>
#include <type_traits>

#ifdef HAVE_LLVM
#include "llvm/ADT/DenseMapInfo.h"
#include "llvm/Support/Hashing.h"
#endif

#include "mem/SubtypeAllocator/SubtypeAllocatorDriver.hpp"
#include "mem/SubtypeAllocator/detail/SubtypeAllocatorDriverBase.hpp"

namespace mem {

template <typename T> class enable_refc_from_this;

namespace detail {

class refc_base {
protected:
  struct counter {
    std::atomic_size_t Ctr;
    size_t Id;
    detail::SubtypeAllocatorDriverBase *Del;

    counter(size_t Ctr, size_t Id,
            detail::SubtypeAllocatorDriverBase *Del) noexcept
        : Ctr(Ctr), Id(Id), Del(Del) {}
  };

  counter *Data = nullptr;

  explicit refc_base(counter *Data) noexcept : Data(Data) {}
  explicit refc_base(const refc_base &Other) noexcept : Data(Other.Data) {}
};

} // namespace detail

/// \brief A reference-counted smart-pointer, similar to \c std::shared_ptr, but
/// optimized for use with SubtypeAllocatorDriver.
///
/// Objects of this type should not be created manually, but using a
/// RefcFactory that handles the internal details. Furthermore, no arrays should
/// be managed with refc. \tparam T The type (or a base type of) the object
/// where this smart-pointer points to
template <typename T> class refc final : public detail::refc_base {

public:
  using counter = detail::refc_base::counter;
  // For internal use only
  struct one_allocation : public counter {
    std::aligned_storage_t<sizeof(T), alignof(T)> Data;

    using counter::counter;
  };

  /// \brief A class that allows allocating a singleton object as refc without a
  /// SubtypeAllocatorDriver.
  ///
  /// Objects of this type can neither be copied, nor moved. The intended usage
  /// is to store them in a static variable and only work with them via a refc
  /// view.
  class singleton : one_allocation {

    friend class refc<T>;

  public:
    /// \brief Creates the wrapped object and forwards \p args to its
    /// constructor
    template <typename... Args>
    singleton(Args &&... args) noexcept(
        std::is_nothrow_constructible_v<T, Args...>)
        : one_allocation(1, detail::SubtypeAllocatorDriverBase::InvalidId,
                         nullptr) {
      new (&this->Data) T(std::forward<Args>(args)...);
    }
    singleton(const singleton &) = delete;
    singleton(singleton &&) = delete;
  };

private:
  explicit refc(one_allocation *Data) noexcept : refc_base(Data) {}
  explicit refc(one_allocation *Data, std::true_type increase_counter) noexcept
      : refc_base(Data) {
    if (Data) {
      Data->Ctr.fetch_add(1, std::memory_order_relaxed);
    }
  }

public:
  refc(std::nullptr_t) noexcept : refc_base(nullptr) {}

  /// \brief Creates a refc from static singleton data
  refc(singleton &Singleton) noexcept : refc(&Singleton, std::true_type{}) {}

  /// \brief For internal use only.
  ///
  /// \param[in] Del The SubtypeAllocatorDriver that should be used for
  /// allocating the object with associated control-block
  /// \param[in] Id The (cached) ID used for allocating a \c
  /// refc<T>::one_allocation with \p Del
  /// \param args The arguments that should be perfectly forwarded to the actual
  /// constructor of \p T.
  template <size_t AllocBlockSize, typename... Args>
  refc(SubtypeAllocatorDriver<AllocBlockSize> *Del,
       detail::SubtypeAllocatorDriverBase::UserAllocatorId Id, Args &&... args)
      : refc_base(nullptr) {
    auto mem = reinterpret_cast<one_allocation *>(Del->allocate(Id));
    auto Ptr = &mem->Data;

    auto Ctr = static_cast<counter *>(mem);
    new (Ctr) counter(1, Id, Del);

    try {
      new (Ptr) T(std::forward<Args>(args)...);
    } catch (...) {
      Del->deallocate(mem, Id);
      // Ptr = nullptr;
      // Ctr = nullptr;
      throw;
    }

    Data = mem;
  }

  /// Copy constructor. Increments the reference-counter by one.
  refc(const refc &Other) noexcept : refc_base(Other.Data) {
    if (Data) {
      assert(Other.Data->Del);
      Data->Ctr.fetch_add(1, std::memory_order_relaxed);
    }
  }

  /// Polymorphic copy constructor. Increments the reference-counter by one.
  template <typename U, typename = std::enable_if_t<std::is_base_of_v<T, U>>>
  refc(const refc<U> &Other) noexcept : refc_base(Other) {
    if (Data) {
      constexpr size_t MagicPointer = 0x10000000000;
      // Unfortunately cannot reinterpret_cast inside a constexpr
      assert(static_cast<T *>(reinterpret_cast<U *>(MagicPointer)) ==
             reinterpret_cast<T *>(MagicPointer));
      assert(Data->Del);
      Data->Ctr.fetch_add(1, std::memory_order_relaxed);
    }
  }

  /// Move constructor. Does not touch the reference-counter. Leaves \p Other in
  /// \c nullptr state.
  refc(refc &&Other) noexcept : refc_base(Other.Data) { Other.Data = nullptr; }

  /// Polymorphic move constructor. Does not touch the reference-counter. Leaves
  /// \p Other in \c nullptr state.
  template <typename U, typename = std::enable_if_t<std::is_base_of_v<T, U>>>
  refc(refc<U> &&Other) noexcept : refc_base(Other) {
    constexpr size_t MagicPointer = 0x10000000000;
    assert(!Data || static_cast<T *>(reinterpret_cast<U *>(MagicPointer)) ==
                        reinterpret_cast<T *>(MagicPointer));

    assert(!Data || Data->Del);
    Other.Data = nullptr;
  }

  /// Destructor. Decrements the reference-counter by one. If it reaches \c 0,
  /// uses the stored SubtypeAllocatorDriver to deallocate the object woth
  /// control-block.
  /// Leaves this object in \c nullptr state
  ~refc() {
    if (!*this)
      return;

    auto dat = static_cast<one_allocation *>(Data);
    Data = nullptr;

    auto oldUseCount = dat->Ctr.fetch_sub(1, std::memory_order_relaxed);
    if (oldUseCount == 1 && dat->Del) {
      auto *dataPtr = reinterpret_cast<T *>(&dat->Data);
      try {
        dataPtr->~T();
      } catch (...) {
        dat->Del->deallocate(dat, dat->Id);
        throw;
      }

      dat->Del->deallocate(dat, dat->Id);
    }
  }

  inline T *get() noexcept {
    return reinterpret_cast<T *>(&static_cast<one_allocation *>(Data)->Data);
  }
  inline const T *get() const noexcept {
    return reinterpret_cast<const T *>(
        &static_cast<one_allocation *>(Data)->Data);
  }

  inline T *operator->() noexcept { return get(); }
  inline const T *operator->() const noexcept { return get(); }

  inline T &operator*() noexcept { return *get(); }
  inline const T &operator*() const noexcept { return *get(); }

  /// Same as *this != nullptr && *this != getEmptyKey() && *this !=
  /// getTombstoneKey()
  operator bool() const noexcept { return size_t(Data + 2) < 3; }

  /// Checks whether this smart-pointer is in the \c nullptr state which means
  /// the pointee cannot be accessed.
  bool operator==(std::nullptr_t) const noexcept { return Data; }

  /// Checks pointer-equality with the Other refc smart pointer.
  /// Fails at compile-time, if \p T and \p U are not in the same inheritance
  /// hierarchy
  template <typename U, typename = std::enable_if_t<std::is_same_v<T, U> ||
                                                    std::is_base_of_v<U, T> ||
                                                    std::is_base_of_v<T, U>>>
  bool operator==(const refc<U> &Other) const noexcept {
    return Data == Other.Data;
  }
  /// Checks pointer-inequality with the Other refc smart pointer
  template <typename U> bool operator!=(const refc<U> &Other) const noexcept {
    return !(*this == Other);
  }

  /// Checks pointer-equality with the Other pointer.
  bool operator==(const T *Other) const noexcept { return get() == Other; }
  /// Checks pointer-inequality with the Other pointer.
  bool operator!=(const T *Other) const noexcept { return !(*this == Other); }

  /// Checks pointer-equality between \p Ptr1 and \p Ptr2
  friend bool operator==(const T *Ptr1, const refc<T> &Ptr2) {
    return Ptr2 == Ptr1;
  }
  /// Checks pointer-inequality between \p Ptr1 and \p Ptr2
  friend bool operator!=(const T *Ptr1, const refc<T> &Ptr2) {
    return Ptr2 != Ptr1;
  }

private:
  friend class enable_refc_from_this<T>;
#ifdef HAVE_LLVM
  friend class llvm::DenseMapInfo<refc<T>>;
#endif

  /// For internal use only.
  ///
  /// Creates an INVALID refc smart pointer for use as empty-key in an
  /// llvm::[Small]Dense{Set,Map}
  static refc getEmptyKey() noexcept { return refc((one_allocation *)-1); }
  /// For internal use only.
  ///
  /// Creates an INVALID refc smart pointer for use as tombstone-key in an
  /// llvm::[Small]Dense{Set,Map}
  static refc getTombstoneKey() noexcept { return refc((one_allocation *)-2); }

  /// For internal use only.
  ///
  /// Creates a refc<T> from the given pointer \p Ptr assuming, but not checking
  /// that it was allocated as refc<T>. Used in the internal implementation from
  /// enable_refc_from_this
  static refc refc_from_this(T *Ptr) {
    if (!Ptr)
      return nullptr;

    constexpr const auto diff = offsetof(one_allocation, Data);

    return refc(reinterpret_cast<one_allocation *>(
                    reinterpret_cast<char *>(Ptr) - diff),
                std::true_type{});
  }
};

template <typename T> class enable_refc_from_this {

public:
  refc<T> refc_from_this() {
    static_assert(std::is_base_of_v<enable_refc_from_this<T>, T>,
                  "Invalid usage of enable_refc_from_this; the template "
                  "parameter must equal the implementing class");
    return refc<T>::refc_from_this(static_cast<T *>(this));
  }
  const refc<T> refc_from_this() const {
    static_assert(std::is_base_of_v<enable_refc_from_this<T>, T>,
                  "Invalid usage of enable_refc_from_this; the template "
                  "parameter must equal the implementing class");
    return refc<T>::refc_from_this(const_cast<T *>(this));
  }
};

#ifdef HAVE_LLVM
template <typename T> llvm::hash_code hash_value(const refc<T> &Rc) {
  return llvm::hash_combine(llvm::hash_value(RC.get()));
}
#endif

} // namespace mem

namespace std {
template <typename T> struct hash<mem::refc<T>> {
  size_t operator()(const mem::refc<T> &Rc) const noexcept {
    constexpr size_t MagicFactor =
        sizeof(size_t) == 32 ? 2654435769UL : 11400714819323198485LLU;
    return std::hash<const T *>()() * MagicFactor;
  }
};
} // namespace std

#ifdef HAVE_LLVM
namespace llvm {
template <typename T> struct DenseMapInfo<mem::refc<T>> {
  static inline mem::refc<T> getEmptyKey() {
    return mem::refc<T>::getEmptyKey();
  }

  static inline mem::refc<T> getTombstoneKey() {
    return mem::refc<T>::getTombstoneKey();
  }

  static inline bool isEqual(const mem::refc<T> &Rc1, const mem::refc<T> &Rc2) {
    return Rc1 == Rc2;
  }

  static inline unsigned getHashValue(const mem::refc<T> &Rc) {
    return hash_value(Rc);
  }
};
} // namespace llvm
#endif