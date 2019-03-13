#ifndef IP4_H
#define IP4_H

#define _BSD_SOURCE 1
#include "serializable.h"

#include <arpa/inet.h>

#include <stdexcept>
#include <string>

using Ip4InvalidInputException = std::runtime_error;

using Ip4 = struct in_addr;

Ip4 Ip4_from_string(const std::string& str);

std::string to_string(const Ip4& ip4);

#endif  // IP4_H
