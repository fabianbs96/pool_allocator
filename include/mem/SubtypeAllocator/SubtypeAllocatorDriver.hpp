#pragma once

#include <optional>

#include "mem/SubtypeAllocator/detail/SubtypeAllocatorDriverBase.hpp"

namespace mem {

/// \brief A pool-allocator that is able to allocate objects of a small range of
/// different types. Does NOT conform to the standard allocator traits.
///
/// This allocator works like a set of PoolAllocators and similarly allocates
/// huge chunks of memory at once - for every supported type in the worst case.
/// Incontrast to the PoolAllocator, this class always uses a free-list to
/// menage "deallocated" objects. For standard allocator trait conformance, use
/// SubtypeAllocator instead and pass a pointer to a SubtypeAllocatorDriver to
/// its constructor.
///
/// \tparam AllocationBlockSize The number of object to allocate at once. The
/// default is 1024
template <size_t AllocationBlockSize = 1024>
class SubtypeAllocatorDriver : public detail::SubtypeAllocatorDriverBase {
  struct Block : public BlockBase {
    char data[0];

    static std::pair<Block *, size_t> create(BlockBase *nxt, size_t ObjectSize,
                                             size_t ObjectAlignment) {
      const auto chunkSize = std::max(ObjectSize, ObjectAlignment);
      const auto offset = std::max(sizeof(Block), ObjectAlignment);
      const auto numBytes = offset + AllocationBlockSize * chunkSize;

      auto ret = reinterpret_cast<Block *>(
          ::new (std::align_val_t{ObjectAlignment}) char[numBytes]);

      ret->next = nxt;

      return {ret, offset - sizeof(Block)};
    }

    static void destroy(BlockBase *Blck, size_t ObjectAlignment) {
      ::operator delete[](reinterpret_cast<char *>(Blck),
                          std::align_val_t{ObjectAlignment});
    }
  };

public:
  using UserAllocatorId = detail::SubtypeAllocatorDriverBase::UserAllocatorId;
  static constexpr UserAllocatorId InvalidId = -1;

  explicit SubtypeAllocatorDriver() noexcept = default;
  SubtypeAllocatorDriver(const SubtypeAllocatorDriver &) = delete;
  SubtypeAllocatorDriver(SubtypeAllocatorDriver &&) = default;
  ~SubtypeAllocatorDriver() {
    size_t i = 0;
    for (auto &config : configs) {
      auto *blck = config.root;
      auto oalign = typeInfos[i].objectAlignment;
      while (blck) {
        auto *next = blck->next;
        Block::destroy(blck, oalign);
        blck = next;
      }
      ++i;
    }
    typeInfos.clear();
    configs.clear();
  }

  /// \brief For internal use only.
  ///
  /// Returns the least number of bytes that are allocated for one object of
  /// type \p T
  template <typename T> static constexpr size_t normalizedSize() {
    return std::max(sizeof(void *), (sizeof(T) + 7)) & ~7;
  }

  /// \brief Computes an ID for use in the actual allocation process
  /// (allocate(UserAllocatorId)). This method takes linear time in the number
  /// of types it has been called for previously.
  template <typename T> UserAllocatorId getId() {
    constexpr auto ObjectSize = sizeof(T);
    constexpr auto ObjectAlignment = alignof(T);
    constexpr auto NormalizedSize = normalizedSize<T>();

    // std::cerr << "> getId(" << ObjectSize << ", " << ObjectAlignment
    //          << ", normalizedSize=" << NormalizedSize << ") = ";

    {
      auto ret = [&]() -> std::optional<UserAllocatorId> {
        auto &ti = typeInfos;
        const auto size = ti.size();
        if (!size)
          return std::nullopt;

        size_t alignment = ~0;
        size_t id = ~0;

        for (size_t i = 0; i < size; ++i) {
          auto [osize, oalign] = ti[i];
          if (osize == NormalizedSize && oalign >= ObjectAlignment) {
            if (oalign < alignment) {
              alignment = oalign;
              id = i;
            }
          }
        }

        if (~id) {
          // found
          // std::cerr << id << std::endl;
          return id;
        }
        return std::nullopt;
      }();

      if (ret.has_value())
        return *ret;
    }
    // none found
    auto ret = typeInfos.size();
    // std::cerr << ret << std::endl;

    // TODO: Maybe do some clever clustering/ordering to prevent the linear
    // search. However, the size of the ti and conf vectors is expected to be
    // very small (5 - 10 at most)
    typeInfos.emplace_back(NormalizedSize, ObjectAlignment);
    configs.emplace_back(nullptr, nullptr, 0, 0);

    return ret;
  }

  /// \brief Allocates an uninitialized chunk of memory large enough for holding
  /// an object with the specified \p Id. The memory chunk is properly aligned
  /// and supports over-alignment.
  void *allocate(UserAllocatorId Id) {
    auto &config = configs[Id];
    // std::cerr << "allocate(" << Id << ") ";

    if (config.freeList) {
      // std::cerr << "uses free-list\n";
      auto ret = config.freeList;
      config.freeList = reinterpret_cast<void **>(*ret);
      return ret;
    }

    auto *blck = config.root;
    auto pos = config.pos;
    const auto last = config.last;
    const auto [osize, oalign] = typeInfos[Id];

    // std::cerr << "ti{ osize=" << osize << ", oalign=" << oalign << " }, ";
    // std::cerr << "cf{ pos=" << pos << ", last=" << last << " } ";

    if (pos + osize > last) {
      // std::cerr << "needs to allocate a new Block\n";
      std::tie(blck, pos) = Block::create(blck, osize, oalign);
      config.root = blck;
      config.last = AllocationBlockSize * osize + pos;
    }

    void *ret = &static_cast<Block *>(blck)->data[pos];
    config.pos = pos + osize;

    return ret;
  }

  //   template <typename T, typename... Args> refc<T> make_refc(Args &&...
  //   args) {
  //     return refc<T>(this, getId<typename refc<T>::one_allocation>(),
  //                    std::forward<Args>(args)...);
  //   }
};
} // namespace mem