#pragma once

#include <array>
#include <tuple>

#include "mem/SubtypeAllocator/SubtypeAllocatorDriver.hpp"
#include "mem/SubtypeAllocator/refc.hpp"
#include "mem/Utility.hpp"

namespace mem {

/// \brief A factory that is able to create objects of the types given in \p Ts
/// and wrap them into a \c mem::refc.
///
/// \tparam AllocBlockSize The number of objects of one size to allocate at
/// once. \tparam Ts The types of objects this factory can allocate.
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
  /// Default constructor. Does not allocate any objects
  explicit RefcFactory() {
    Ids = initializeIds(std::make_index_sequence<sizeof...(Ts)>{});
    // for (auto id : Ids) {
    //   std::cerr << id << " ";
    // }
    // std::cerr << std::endl;
  }

  /// Constructor. Can control, how much space should be available initially for
  /// each type. \param initialCapacities This array contains an
  /// initialCapacity
  /// for each type from \p Ts. Initially allocates enough space for creating at
  /// least \c initialCapacity objects of the respective types before allocating
  /// more memory.
  explicit RefcFactory(
      const std::array<size_t, sizeof...(Ts)> &initialCapacities) {
    Ids = initializeIds(std::make_index_sequence<sizeof...(Ts)>{});

    std::array<size_t, sizeof...(Ts)> initCapById;
    initCapById.fill(0);

    const size_t numIds = Driver.getNumIds();

    for (size_t i = 0; i < initCapById.size(); ++i) {
      auto currId = Ids[i];
      initCapById[currId] += initialCapacities[i];
    }

    for (size_t i = 0; i < numIds; ++i) {
      if (initCapById[i])
        Driver.reserve(initCapById[i]);
    }
  }

  /// \brief Creates an object of type \p U and forwards the arguments \p args
  /// to \p U's constructor.
  /// \returns The newly created object wrapped into a \c refc
  template <typename U, typename... Args> refc<U> create(Args &&... args) {
    auto id = Ids[tuple_index_v<U, Ts...>];
    return refc<U>(&Driver, id, std::forward<Args>(args)...);
  }
};
} // namespace mem