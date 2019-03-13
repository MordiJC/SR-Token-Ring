#include "utility.h"


void insertStringToCharArrayWithLength(const std::string &str, char *array, size_t maxLen) {
  std::memset(array, 0, maxLen);
  std::memcpy(array, str.c_str(), std::min(maxLen - 1, str.size()));
}

std::string maybeNonterminatedCharArrayToString(const char *array, size_t maxLen) {
  if (array[maxLen - 1] == 0) {
    return std::string(array);
  } else {
    return std::string(array, array + maxLen);
  }
}
