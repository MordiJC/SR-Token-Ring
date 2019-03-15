#include "logger.h"

#include <cstring>
#include <mutex>

Logger *Logger::instance = nullptr;

Ip4 Logger::multicastIp;

std::string Logger::multicastIpStr = "225.225.225.225";
unsigned short Logger::multicastPort = 2525;

Logger::Logger() { outputSocket = std::make_unique<Socket>(Protocol::UDP); }

Logger &Logger::getInstance() {
  if (!instance) {
    Logger::multicastIp = Ip4_from_string(multicastIpStr);
    instance = new Logger();
  }

  return *instance;
}

void Logger::log(const std::string &msg) {
  {
    std::lock_guard<std::mutex> coutGuard(coutMutex);
    std::cout << msg << std::endl;
  }

  {
    std::lock_guard<std::mutex> socketGuard(socketMutex);

    std::vector<unsigned char> buffer;
    buffer.resize(msg.size());

    std::memcpy(buffer.data(), msg.c_str(), msg.size());

    outputSocket->sendTo(buffer, multicastIp, multicastPort);
  }
}
