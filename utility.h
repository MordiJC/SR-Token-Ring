#ifndef UTILITY_H
#define UTILITY_H

#include <algorithm>
#include <cstring>
#include <random>
#include <set>
#include <string>

void insertStringToCharArrayWithLength(const std::string& str, char* array,
                                       size_t maxLen);

std::string maybeNonterminatedCharArrayToString(const char* array,
                                                size_t maxLen);

template <typename T>
T random(T min, T max) {
  static std::mt19937 gen{std::random_device{}()};
  using dist = std::conditional_t<std::is_integral<T>::value,
                                  std::uniform_int_distribution<T>,
                                  std::uniform_real_distribution<T> >;
  return dist{min, max}(gen);
}

template <typename T>
std::pair<T, bool> getNthElement(std::set<T>& searchSet, int n) {
  std::pair<T, bool> result;
  if (searchSet.size() > n) {
    result.first = *(std::next(searchSet.begin(), n));
    result.second = true;
  } else
    result.second = false;

  return result;
}

#endif  // UTILITY_H
