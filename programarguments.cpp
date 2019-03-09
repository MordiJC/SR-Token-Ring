#include "programarguments.h"

#include <algorithm>
#include <limits>

ProgramArguments::ProgramArguments(std::vector<const char *> arguments,
                                   bool shouldParse) noexcept(false)
    : arguments(arguments) {
  if (shouldParse) {
    parse();
  }
}

void ProgramArguments::parseUserPort(const std::string &input) {
  int userPort = 0;

  try {
    userPort = std::stoi(input);
  } catch (const std::invalid_argument &) {
    throw ProgramArgumentsInvalidPortNumberException(
        std::string("Invalid port number passed `") + input + "\'");
  }

  if (userPort < 0 || userPort > std::numeric_limits<unsigned short>::max()) {
    throw ProgramArgumentsInvalidPortNumberException(
        std::string("Invalid port number passed `") + input + "\'");
  }

  port = static_cast<unsigned short>(userPort);
}

void ProgramArguments::parseNeighborIp(const std::string &input) {
  try {
    Ip4 ip(input);
    neighborIp = ip;
  } catch (const Ip4InvalidInputException &ex) {
    throw ProgramArgumentsInvalidNeighborIpAddressException(ex.what());
  }
}

void ProgramArguments::parseNeighborPort(const std::string &input) {
  int userPort = 0;

  try {
    userPort = std::stoi(input);
  } catch (const std::invalid_argument &) {
    throw ProgramArgumentsInvalidPortNumberException(
        std::string("Invalid neighbor port number passed `") + input + "\'");
  }

  if (userPort < 0 || userPort > std::numeric_limits<unsigned short>::max()) {
    throw ProgramArgumentsInvalidPortNumberException(
        std::string("Invalid neighbor port number passed `") + input + "\'");
  }

  neighborPort = static_cast<unsigned short>(userPort);
}

void ProgramArguments::parseTokenStatus(const std::string &input) {
  std::string tokenStatusStr = input;

  std::transform(tokenStatusStr.begin(), tokenStatusStr.end(),
                 tokenStatusStr.begin(), ::tolower);
  if (tokenStatusStr == "true" || tokenStatusStr == "t" ||
      tokenStatusStr == "y" || tokenStatusStr == "yes" ||
      tokenStatusStr == "1") {
    hasToken = true;
  } else if (tokenStatusStr == "false" || tokenStatusStr == "f" ||
             tokenStatusStr == "n" || tokenStatusStr == "no" ||
             tokenStatusStr == "0") {
    hasToken = false;
  } else {
    throw ProgramArgumentsInvalidTokenStatusException(
        "Invalid token status passed `" + tokenStatusStr + "\'");
  }
}

void ProgramArguments::parseProtocol(const std::string &input) {
  std::string protocolStr = input;
  std::transform(protocolStr.begin(), protocolStr.end(), protocolStr.begin(),
                 ::tolower);
  if (protocolStr == "tcp") {
    protocol = Protocol::TCP;
  } else if (protocolStr == "udp") {
    protocol = Protocol::UDP;
  } else {
    throw ProgramArgumentsInvalidProtocolException("Invalid protocol passed `" +
                                                   protocolStr + "\'");
  }
}

void ProgramArguments::parse() {
  if (arguments.size() < 6) {
    throw ProgramArgumentsNotEnoughArgumentsException(
        "Not enough arguments passed");
  }

  userIdentifier = arguments[0];

  parseUserPort(arguments[1]);

  parseNeighborIp(arguments[2]);

  parseNeighborPort(arguments[3]);

  parseTokenStatus(arguments[4]);

  parseProtocol(arguments[5]);
}

std::string ProgramArguments::getUserIdentifier() const {
  return userIdentifier;
}

unsigned short ProgramArguments::getPort() const { return port; }

Ip4 ProgramArguments::getNeighborIp() const { return neighborIp; }

unsigned short ProgramArguments::getNeighborPort() const {
  return neighborPort;
}

bool ProgramArguments::getHasToken() const { return hasToken; }

Protocol ProgramArguments::getProtocol() const { return protocol; }

std::vector<const char *> ProgramArguments::getArguments() const {
  return arguments;
}

bool ProgramArguments::isInputParsed() const { return inputParsed; }
