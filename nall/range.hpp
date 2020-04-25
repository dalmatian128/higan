#pragma once

namespace nall {

template <typename T>
struct range_t {
  using utype = std::conditional_t<std::is_arithmetic_v<T>, T, s64>;
  struct iterator {
    iterator(utype position, utype step = 0) : position(position), step(step) {}
    auto operator*() const -> utype { return position; }
    auto operator!=(const iterator& source) const -> bool { return step > 0 ? position < source.position : position > source.position; }
    auto operator++() -> iterator& { position += step; return *this; }

  private:
    utype position;
    const utype step;
  };

  struct reverse_iterator {
    reverse_iterator(utype position, utype step = 0) : position(position), step(step) {}
    auto operator*() const -> utype { return position; }
    auto operator!=(const reverse_iterator& source) const -> bool { return step > 0 ? position > source.position : position < source.position; }
    auto operator++() -> reverse_iterator& { position -= step; return *this; }

  private:
    utype position;
    const utype step;
  };

  auto begin() const -> iterator { return {origin, stride}; }
  auto end() const -> iterator { return {target}; }

  auto rbegin() const -> reverse_iterator { return {target - stride, stride}; }
  auto rend() const -> reverse_iterator { return {origin - stride}; }

  utype origin;
  utype target;
  utype stride;
};

template <typename T>
inline auto range(T size) {
  return range_t<T>{0, size, 1};
}

template <typename T>
inline auto range(T offset, T size) {
  return range_t<T>{offset, size, 1};
}
template <typename T, typename U>
inline auto range(T offset, U size) {
  return range_t<T>{offset, static_cast<T>(size), 1};
}

template <typename T>
inline auto range(T offset, T size, T step) {
  return range_t<T>{offset, size, step};
}

}
