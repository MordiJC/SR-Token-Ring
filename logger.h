#ifndef LOGGER_H
#define LOGGER_H

#include "ip4.h"
#include "socket.h"

#include <iostream>
#include <memory>
#include <mutex>

class Logger {
 private:
  static Logger* instance;
  static Ip4 multicastIp;

 public:
  static std::string multicastIpStr;
  static unsigned short multicastPort;

 private:
  std::unique_ptr<Socket> outputSocket;
  std::mutex socketMutex;
  std::mutex coutMutex;

 private:
  Logger();

 public:
  static Logger& getInstance();

  void log(const std::string& msg);
};

#endif  // LOGGER_H
