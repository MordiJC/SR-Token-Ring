#ifndef IP4_H
#define IP4_H

#define _BSD_SOURCE 1
#include "serializable.h"

#include <arpa/inet.h>

#include <stdexcept>
#include <string>

using Ip4InvalidInputException = std::runtime_error;

class Ip4: public Serializable {
 private:
  struct in_addr address;

 public:
  Ip4() noexcept;

  explicit Ip4(struct in_addr& socketAddress) noexcept;

  explicit Ip4(const std::string& inputIpAddress) noexcept(false);

  in_addr getAddress() const;

  std::string to_string() const;

  static const size_t SIZE = sizeof(address);

  // Serializable interface
public:
  Serializable::size_type fromBinary(const Serializable::container_type &buffer);
  Serializable::container_type toBinary() const;
};

#endif  // IP4_H
