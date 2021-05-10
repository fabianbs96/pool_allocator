#pragma once

#include <vector>

namespace mem {
namespace detail {
class SubtypeAllocatorDriverBase {
protected:
  struct TypeInfo {
    size_t objectSize;
    size_t objectAlignment;

    TypeInfo(size_t ObjectSize, size_t ObjectAlignment) noexcept
        : objectSize(ObjectSize), objectAlignment(ObjectAlignment) {}

    friend inline bool operator==(const TypeInfo &C1,
                                  const TypeInfo &C2) noexcept {
      return C1.objectSize == C2.objectSize &&
             C1.objectAlignment == C2.objectAlignment;
    }

    friend inline bool operator!=(const TypeInfo &C1,
                                  const TypeInfo &C2) noexcept {
      return !(C1 == C2);
    }
  };

  struct BlockBase {
    BlockBase *next = nullptr;
  };

  struct Config {
    BlockBase *root;
    void **freeList;
    size_t pos, last;

    Config(BlockBase *Root, void **FreeList, size_t Pos, size_t Last) noexcept
        : root(Root), freeList(FreeList), pos(Pos), last(Last) {}
  };

  std::vector<TypeInfo> typeInfos;
  std::vector<Config> configs;

public:
  using UserAllocatorId = size_t;
  static constexpr UserAllocatorId InvalidId = -1;

  inline void deallocate(void *Obj, UserAllocatorId Id) noexcept {
    // std::cerr << "deallocate(" << Id << ")\n";
    auto freeList = configs[Id].freeList;
    // Obj has at least one pointer-size (See the definition of NormalizedSize
    // in getId())
    auto nwFL = reinterpret_cast<void **>(Obj);
    *nwFL = freeList;
    configs[Id].freeList = nwFL;
  }

  size_t getNumIds() const noexcept { return typeInfos.size(); }
};
} // namespace detail
} // namespace mem