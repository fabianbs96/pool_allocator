#pragma once

#include <cstddef> // size_t

namespace mem {

template <size_t AllocationBlockSize> class SubtypeAllocatorDriver;

namespace detail {
template <size_t AllocationBlockSize> class SubtypeAllocatorBase {
  // protected:
public:
  SubtypeAllocatorDriver<AllocationBlockSize> *Driver;
  typename SubtypeAllocatorDriver<AllocationBlockSize>::UserAllocatorId Id =
      -1;

public:
  SubtypeAllocatorBase(
      SubtypeAllocatorDriver<AllocationBlockSize> *Driver) noexcept
      : Driver(Driver) {}
  SubtypeAllocatorBase(
      SubtypeAllocatorDriver<AllocationBlockSize> *Driver,
      typename SubtypeAllocatorDriver<AllocationBlockSize>::UserAllocatorId
          Id) noexcept
      : Driver(Driver), Id(Id) {}

  friend bool operator==(const SubtypeAllocatorBase &PMA1,
                         const SubtypeAllocatorBase &PMA2) noexcept {
    return PMA1.Driver == PMA2.Driver && PMA1.Id == PMA2.Id;
  }
  friend bool operator!=(const SubtypeAllocatorBase &PMA1,
                         const SubtypeAllocatorBase &PMA2) noexcept {
    return !(PMA1 == PMA2);
  }
};

} // namespace detail
} // namespace mem