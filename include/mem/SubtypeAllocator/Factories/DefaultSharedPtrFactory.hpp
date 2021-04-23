#pragma once

#include <memory>

#include "mem/Utility.hpp"

namespace mem {
template <typename... Ts> class DefaultSharedPtrFactory final {
public:
  explicit DefaultSharedPtrFactory() = default;

  template <typename U, typename... Args>
  std::shared_ptr<U> create(Args &&... args) {
    // Trigger a compilation-error whenever this method is called for the wrong
    // type
    auto unused = tuple_index_v<U, Ts...>;

    return std::make_shared<U>(std::forward<Args>(args)...);
  }
};
} // namespace mem