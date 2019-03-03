#ifndef COMMON_H
#define COMMON_H

#include <cassert>
#include <iostream>
#include <memory>
#include <type_traits>
#include <utility>
#include <vector>

namespace lang {

template <typename T>
void SafeSignedInc(T &x) {
  static_assert(
      std::is_signed<T>::value,
      "Can only perform a safe signed increment on a signed integral type");
  ++x;
  assert(x >= 0 && "Overflowed on incrementing signed integral number");
}

template <typename T, typename U>
void SafeSignedInplaceAdd(T &x, U y) {
  static_assert(
      std::is_signed<T>::value,
      "Can only perform a safe signed inplace add on a signed integral type");
  x += y;
  assert(x >= 0 && "Overflowed on inplace adding signed integral number");
}

template <typename T, typename... Args>
T *SafeNew(Args &&... args) {
  T *result = new T(std::forward<Args>(args)...);
  assert(result && "Ran out of memory when creating new type");
  return result;
}

// This is so much simpler and less space-consuming this way.
template <typename T>
using unique = std::unique_ptr<T>;

template <typename T>
void CheckNonNull(T *ptr) {
  assert(ptr && "Did not expect a nullptr");
}

template <typename T>
void CheckNonNullVector(const std::vector<unique<T>> &vec) {
  for (const auto &ptr : vec) CheckNonNull(ptr.get());
}

}  // namespace lang

#endif
