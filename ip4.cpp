#include "ip4.h"

Ip4::Ip4() noexcept : address({0}) {}

Ip4::Ip4(in_addr& socketAddress) noexcept : address(socketAddress) {}

Ip4::Ip4(const std::string& inputIpAddress) noexcept(false) {
  int ret = inet_aton(inputIpAddress.c_str(), &address);

  if (ret == 0) {
    throw Ip4InvalidInputException(std::string("Invalid input adress `") +
                                   inputIpAddress + "\'");
  }
}

in_addr Ip4::getAddress() const { return address; }

std::string Ip4::to_string() const { return std::string(inet_ntoa(address)); }
