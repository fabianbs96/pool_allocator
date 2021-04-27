#pragma once

#include <memory>

#include "mem/SubtypeAllocator/SubtypeAllocatorDriver.hpp"
#include "mem/Utility.hpp"

namespace mem {

namespace detail {
template <size_t AllocBlockSize> class SharedPtrFactoryAllocatorBase {
protected:
  SubtypeAllocatorDriver<AllocBlockSize> *Driver;
  typename SubtypeAllocatorDriver<AllocBlockSize>::UserAllocatorId *Id;

public:
  explicit SharedPtrFactoryAllocatorBase(
      SubtypeAllocatorDriver<AllocBlockSize> *Driver,
      typename SubtypeAllocatorDriver<AllocBlockSize>::UserAllocatorId *Id)
      : Driver(Driver), Id(Id) {}

  SharedPtrFactoryAllocatorBase(const SharedPtrFactoryAllocatorBase &) =
      default;
  SharedPtrFactoryAllocatorBase(SharedPtrFactoryAllocatorBase &&) = default;
};

template <typename T, size_t AllocBlockSize>
class SharedPtrFactoryAllocator final
    : public SharedPtrFactoryAllocatorBase<AllocBlockSize> {

public:
  explicit SharedPtrFactoryAllocator(
      SubtypeAllocatorDriver<AllocBlockSize> *Driver,
      typename SubtypeAllocatorDriver<AllocBlockSize>::UserAllocatorId *Id)
      : SharedPtrFactoryAllocatorBase<AllocBlockSize>(Driver, Id) {}

  SharedPtrFactoryAllocator(const SharedPtrFactoryAllocator &) = default;
  SharedPtrFactoryAllocator(SharedPtrFactoryAllocator &&) = default;

  template <typename U>
  SharedPtrFactoryAllocator(
      const SharedPtrFactoryAllocator<U, AllocBlockSize> &Other)
      : SharedPtrFactoryAllocatorBase<AllocBlockSize>(Other) {}

  template <typename U>
  SharedPtrFactoryAllocator(
      SharedPtrFactoryAllocator<U, AllocBlockSize> &&Other)
      : SharedPtrFactoryAllocatorBase<AllocBlockSize>(std::move(Other)) {}

  using size_type = size_t;
  using difference_type = std::ptrdiff_t;
  using pointer = T *;
  using const_pointer = const T *;
  using reference = T &;
  using const_reference = const T &;
  using value_type = T;

  template <typename U> struct rebind {
    using other = SharedPtrFactoryAllocator<U, AllocBlockSize>;
  };

  pointer allocate(size_t N) {
    if (N != 1) {
      // cannot allocate arrays, since the Blocks are not contiguous
      // in memory. So, fallback to the default allocator
      return reinterpret_cast<pointer>(
          ::new std::aligned_storage_t<sizeof(T), alignof(T)>[N]);
    }

    auto id = *this->Id;
    if (__builtin_expect(id == this->Driver->InvalidId, false)) {
      *this->Id = id = this->Driver->template getId<T>();
    }

    return reinterpret_cast<pointer>(this->Driver->allocate(id));
  }

  void deallocate(pointer Ptr, size_t N) {
    if (N != 1) {
      ::delete[] reinterpret_cast<
          std::aligned_storage_t<sizeof(T), alignof(T)> *>(Ptr);
      return;
    }

    auto id = *this->Id;
    if (__builtin_expect(id == this->Driver->InvalidId, false)) {
      *this->Id = id = this->Driver->template getId<T>();
    }

    this->Driver->deallocate(Ptr, id);
  }
};
} // namespace detail

template <size_t AllocBlockSize, typename... Ts> class SharedPtrFactory final {
  SubtypeAllocatorDriver<AllocBlockSize> Driver;
  std::array<typename SubtypeAllocatorDriver<AllocBlockSize>::UserAllocatorId,
             sizeof...(Ts)>
      Ids;

public:
  /// Default constructor. Does not allocate any objects
  explicit SharedPtrFactory() {
    Ids.fill(SubtypeAllocatorDriver<AllocBlockSize>::InvalidId);
  }

  /// \brief Creates an object of type \p U and forwards the arguments \p args
  /// to \p U's constructor.
  /// \returns The newly created object wrapped into a \c std::shared_ptr
  template <typename U, typename... Args>
  std::shared_ptr<U> create(Args &&... args) {
    detail::SharedPtrFactoryAllocator<U, AllocBlockSize> Alloc(
        &Driver, &Ids[tuple_index_v<U, Ts...>]);
    return std::allocate_shared<U>(Alloc, std::forward<Args>(args)...);
  }
};
} // namespace mem
