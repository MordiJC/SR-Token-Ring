#include "ip4.h"

#include <cstring>
#include <string>

Ip4 Ip4_from_string(const std::string& str) {
  Ip4 address;
  int ret = inet_aton(str.c_str(), &address);

  if (ret == 0) {
    throw Ip4InvalidInputException(std::string("Invalid input adress `") + str +
                                   "\'");
  }

  return address;
}

std::string to_string(const Ip4& ip4) { return std::string(inet_ntoa(ip4)); }
