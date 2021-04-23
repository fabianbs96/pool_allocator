#pragma once
#include <iostream>
#include <memory>
#include <stdexcept>
#include <type_traits>

namespace mem {

/// \brief A simple pool-allocator that is able to allocate objects of a fixed
/// type \p T. Conforms to the standard allocator traits.
///
/// It works by allocating huge chunks of memory at once to reduce the number of
/// actual allocations and to increase data locality of the allocated objects.
/// Since this allocator may use a free-list to maintain "deallocated" object
/// chunks, array-allocation will be delegated directly to the standard C++
/// allocator. A typical use-case is to speedup node-based containers like \c
/// std::list, \c std::set, etc. \tparam T The type of objects to allocate
/// \tparam UseFreeList \c true, if the allocator should be able to deallocate
/// objects before destructing itself. The default is \c true
/// \tparam BlockSize The number of objects that shouold be allocated at once.
/// The default is \c 1024 \remarks Compare with
/// https://stackoverflow.com/a/24289614
template <typename T, bool UseFreeList = true, unsigned BlockSize = 1024>
class PoolAllocator {
  static_assert(BlockSize != 0, "The BlockSize must not be 0");
  struct Block {
    union DataField {
      DataField *nextFree;
      T data;
    };

    using value_type =
        std::aligned_storage_t<sizeof(DataField), alignof(DataField)>;
    Block *next;
    value_type data[0];

    static Block *create(Block *nxt, unsigned n) {
      // sizeof(Block) already contains the padding between Block::next and
      // Block::data
      auto ret = reinterpret_cast<Block *>(::new (std::align_val_t{
          alignof(Block)}) uint8_t[sizeof(Block) + (n * sizeof(value_type))]);
      ret->next = nxt;
      return ret;
    }
  };

  template <bool FL> struct MemoryPool;
  template <> struct MemoryPool<true> {
    Block *pool;
    typename Block::DataField *freeList;

    MemoryPool() noexcept : pool(nullptr), freeList(nullptr) {}
    MemoryPool(std::nullptr_t) noexcept : MemoryPool() {}
  };
  template <> struct MemoryPool<false> {
    Block *pool;
    MemoryPool() noexcept : pool(nullptr) {}
    MemoryPool(std::nullptr_t) noexcept : MemoryPool() {}
  };

  MemoryPool<UseFreeList> mpool;

  unsigned currBlockSize;
  unsigned index;

public:
  PoolAllocator(unsigned reserved = BlockSize) noexcept
      : mpool(), index(reserved),
        currBlockSize(reserved){
            // std::cout << "> Ctor(reserved=" << reserved << ")\n";
        };

  PoolAllocator(const PoolAllocator &other) noexcept
      : PoolAllocator(other.currBlockSize) {}
  template <typename U, bool FL>
  PoolAllocator(const PoolAllocator<U, FL> &other) noexcept
      : PoolAllocator(other.minCapacity()) {}

  PoolAllocator(PoolAllocator &&other) noexcept
      : mpool(other.mpool), index(other.index),
        currBlockSize(other.currBlockSize) {
    other.mpool = nullptr;
  }

  PoolAllocator &operator=(const PoolAllocator &other) noexcept = delete;
  PoolAllocator &operator=(PoolAllocator &&other) noexcept {
    this->PoolAllocator::~PoolAllocator();
    ::new (this) PoolAllocator(std::move(other));
    return *this;
  }

  ~PoolAllocator() {
    // The data inside the blocks is assumed to be already destroyed.
    for (auto p = mpool.pool; p;) {
      auto nxt = p->next;

      ::operator delete[](reinterpret_cast<uint8_t *>(p),
                          std::align_val_t{alignof(Block)});
      //::delete[] reinterpret_cast<uint8_t *>(p);
      p = nxt;
    }

    mpool = nullptr;
  }

  using size_type = size_t;
  using difference_type = std::ptrdiff_t;
  using pointer = T *;
  using const_pointer = const T *;
  using reference = T &;
  using const_reference = const T &;
  using value_type = T;
  using propagate_on_container_move_assignment = std::true_type;

  template <class U> struct rebind {
    using other = PoolAllocator<U, UseFreeList, BlockSize>;
  };

  pointer allocate(size_t n) {
    if (n != 1) {
      // cannot allocate arrays, since the Blocks are not contiguous
      // in memory. So, fallback to the default allocator
      return reinterpret_cast<pointer>(
          ::new std::aligned_storage_t<sizeof(T), alignof(T)>[n]);
    }
    if constexpr (UseFreeList) {
      if (mpool.freeList) {
        auto fld = mpool.freeList;
        mpool.freeList = fld->nextFree;
        return reinterpret_cast<pointer>(fld);
      }
    }

    if (index == currBlockSize) {

      Block *nwPl;
      if (mpool.pool) {
        // std::cout << "> Allocate " << BlockSize << " elements" << std::endl;
        nwPl = Block::create(mpool.pool, BlockSize);

        if (currBlockSize != BlockSize)
          currBlockSize = BlockSize;
      } else {
        // std::cout << "> Allocate " << currBlockSize << " elements" <<
        // std::endl;
        nwPl = Block::create(mpool.pool, currBlockSize);
      }
      mpool.pool = nwPl;
      index = 1;
      return reinterpret_cast<pointer>(&nwPl->data[0]);
    }

    return reinterpret_cast<pointer>(&mpool.pool->data[index++]);
  }

  void deallocate(pointer ptr, size_t n) {
    if (n != 1) {
      ::delete[] reinterpret_cast<
          std::aligned_storage_t<sizeof(T), alignof(T)> *>(ptr);
      return;
    }
    if constexpr (UseFreeList) {
      // Only insert the pointer into the free-list. Actual deallocation happens
      // in the destructor of this allocator.
      auto *fl = reinterpret_cast<typename Block::DataField *>(ptr);
      fl->nextFree = mpool.freeList;
      mpool.freeList = fl;
    }
  }

  template <typename... Args> void construct(pointer ptr, Args &&... args) {
    ::new (ptr) T(std::forward<Args>(args)...);
  }
  void destroy(pointer ptr) noexcept(std::is_nothrow_destructible_v<T>) {
    ptr->T::~T();
  }

  bool operator==(const PoolAllocator &other) const noexcept { return true; }
  bool operator!=(const PoolAllocator &other) const noexcept {
    return !(*this == other);
  }

  // For internal use only
  unsigned minCapacity() const noexcept { return currBlockSize; }
};
} // namespace mem