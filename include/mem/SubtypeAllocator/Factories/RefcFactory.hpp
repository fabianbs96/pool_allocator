#pragma once

#include <tuple>

#include "mem/SubtypeAllocator/SubtypeAllocatorDriver.hpp"
#include "mem/SubtypeAllocator/refc.hpp"
#include "mem/Utility.hpp"

namespace mem {

template <size_t AllocBlockSize, typename... Ts> class RefcFactory final {
  SubtypeAllocatorDriver<AllocBlockSize> Driver;
  std::array<typename SubtypeAllocatorDriver<AllocBlockSize>::UserAllocatorId,
             sizeof...(Ts)>
      Ids;

  template <size_t... Ns>
  std::array<size_t, sizeof...(Ns)> initializeIds(std::index_sequence<Ns...>) {
    return {Driver.template getId<typename refc<
        std::tuple_element_t<Ns, std::tuple<Ts...>>>::one_allocation>()...};
  }

public:
  explicit RefcFactory() {
    Ids = initializeIds(std::make_index_sequence<sizeof...(Ts)>{});
    // for (auto id : Ids) {
    //   std::cerr << id << " ";
    // }
    // std::cerr << std::endl;
  }

  template <typename U, typename... Args> refc<U> create(Args &&... args) {
    auto id = Ids[tuple_index_v<U, Ts...>];
    return refc<U>(&Driver, id, std::forward<Args>(args)...);
  }
};
} // namespace mem