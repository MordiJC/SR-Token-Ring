#ifndef PROGRAMARGUMENTS_H
#define PROGRAMARGUMENTS_H

#include <stdexcept>
#include <string>
#include <vector>

#include "ip4.h"
#include "protocol.h"

/* Exceptions */

using ProgramArgumentsException = std::runtime_error;

using ProgramArgumentsNotEnoughArgumentsException = ProgramArgumentsException;

using ProgramArgumentsInvalidPortNumberException = ProgramArgumentsException;

using ProgramArgumentsInvalidNeighborIpAddressException =
    ProgramArgumentsException;

using ProgramArgumentsInvalidTokenStatusException = ProgramArgumentsException;

using ProgramArgumentsInvalidProtocolException = ProgramArgumentsException;

/* Class declaration */

class ProgramArguments {
 private:
  std::string userIdentifier;
  unsigned short port;
  Ip4 neighborIp;
  unsigned short neighborPort;
  bool hasToken = false;
  Protocol protocol = Protocol::NONE;

  std::vector<const char *> arguments;
  bool inputParsed = false;

  void parseUserPort(const std::string &input);

  void parseNeighborIp(const std::string &input);

  void parseNeighborPort(const std::string &input);

  void parseTokenStatus(const std::string &input);

  void parseProtocol(const std::string &input);

 public:
  ProgramArguments() = delete;

  explicit ProgramArguments(std::vector<const char *> arguments,
                            bool shouldParse = false) noexcept(false);

  void parse() noexcept(false);

  std::string getUserIdentifier() const;

  unsigned short getPort() const;

  Ip4 getNeighborIp() const;

  unsigned short getNeighborPort() const;

  bool getHasToken() const;

  Protocol getProtocol() const;

  std::vector<const char *> getArguments() const;

  bool isInputParsed() const;
};

#endif  // PROGRAMARGUMENTS_H
