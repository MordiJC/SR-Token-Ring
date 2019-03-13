#ifndef UTILITY_H
#define UTILITY_H

#include <algorithm>
#include <cstring>
#include <string>

void insertStringToCharArrayWithLength(const std::string& str, char* array,
                                       size_t maxLen);

std::string maybeNonterminatedCharArrayToString(const char* array,
                                                size_t maxLen);

#endif  // UTILITY_H
