#pragma once
#include <tuple>

namespace mem {
namespace detail {
template <typename... T> struct tuple_index {
  static_assert(sizeof...(T) > 1,
                "Out Of Bounds: The index-type is not present in the tuple");
};

template <typename T, typename... Us> struct tuple_index<T, T, Us...> {
  enum { value = 0 };
};
template <typename T, typename U, typename... Us>
struct tuple_index<T, U, Us...> {
  enum { value = 1 + tuple_index<T, Us...>::value };
};

} // namespace detail

template <typename T, typename... Us>
constexpr size_t tuple_index_v = detail::tuple_index<T, Us...>::value;
} // namespace mem