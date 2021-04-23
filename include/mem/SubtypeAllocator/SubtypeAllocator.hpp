#pragma once

#include <type_traits> //aligned_storage

#include "mem/SubtypeAllocator/SubtypeAllocatorDriver.hpp"
#include "mem/SubtypeAllocator/detail/SubtypeAllocatorBase.hpp"

namespace mem {

/// \brief A standard allocator trait conformant wrapper over a pointer to a
/// SubtypeAllocatorDriver. Its intended usage is for \c std::allocate_shared.
///
/// \tparam T The type of objects this allocator should allocate
/// \tparam AllocationBlockSize The \c AllocationBlockSize used in the wrapped
/// SubtypeAllocatorDriver
template <typename T, size_t AllocationBlockSize = 1024>
class SubtypeAllocator
    : public detail::SubtypeAllocatorBase<AllocationBlockSize> {
  template <typename U>
  static constexpr size_t normalized_size_v =
      SubtypeAllocatorDriver<AllocationBlockSize>::template normalizedSize<U>();

public:
  SubtypeAllocator(SubtypeAllocatorDriver<AllocationBlockSize> *Driver) noexcept
      : detail::SubtypeAllocatorBase<AllocationBlockSize>(Driver) {}
  SubtypeAllocator(const SubtypeAllocator &Other) noexcept
      : detail::SubtypeAllocatorBase<AllocationBlockSize>(Other.Driver,
                                                          Other.Id) {
    // std::cerr << "Copy allocator with id " << this->Id << "\n";
  }
  template <typename U>
  SubtypeAllocator(
      const SubtypeAllocator<U, AllocationBlockSize> &Other) noexcept
      : detail::SubtypeAllocatorBase<AllocationBlockSize>(
            Other.Driver, normalized_size_v<U> == normalized_size_v<T> &&
                                  alignof(U) == alignof(T)
                              ? Other.Id
                              : -1) {

    // std::cerr << "Rebind allocator with id " << Other.Id << "\n";
  }

  using size_type = size_t;
  using difference_type = std::ptrdiff_t;
  using pointer = T *;
  using const_pointer = const T *;
  using reference = T &;
  using const_reference = const T &;
  using value_type = T;

  template <typename U> struct rebind {
    using other = SubtypeAllocator<U, AllocationBlockSize>;
  };

  pointer allocate(size_t N) {
    if (N != 1) {
      // cannot allocate arrays, since the Blocks are not contiguous
      // in memory. So, fallback to the default allocator
      return reinterpret_cast<pointer>(
          ::new std::aligned_storage_t<sizeof(T), alignof(T)>[N]);
    }

    auto id = this->Id;
    if (id == -1) {
      this->Id = id = this->Driver->template getId<T>();
    }

    return reinterpret_cast<pointer>(this->Driver->allocate(id));
  }

  void deallocate(pointer Ptr, size_t N) {
    if (N != 1) {
      ::delete[] reinterpret_cast<
          std::aligned_storage_t<sizeof(T), alignof(T)> *>(Ptr);
      return;
    }

    auto id = this->Id;
    if (id == -1) {
      this->Id = id = this->Driver->template getId<T>();
    }

    this->Driver->deallocate(Ptr, id);
  }
};

} // namespace mem