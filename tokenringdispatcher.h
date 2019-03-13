#ifndef TOKENRINGDISPATCHER_H
#define TOKENRINGDISPATCHER_H

#include <memory>
#include <queue>
#include <random>
#include <set>
#include <stdexcept>

#include "programarguments.h"
#include "socket.h"
#include "tokenringpacket.h"

using TokenRingDispatcherException = std::runtime_error;

using TokenRingDispatcherNotEnoughDataForRegisterSubheaderException =
    TokenRingDispatcherException;

class TokenRingDispatcher {
 public:
  static const std::string workingIp;

 private:
  std::queue<TokenRingPacket> registerRequestPackets;  // First priority
  std::queue<TokenRingPacket> dataPackets;             // Second priority

  std::unique_ptr<Socket> outputSocket;
  std::unique_ptr<Socket> neighborSocket;

  std::string neighborName;

  Ip4 neighborIp;
  unsigned short neighborPort = 0;

  ProgramArguments programArguments;

  bool hasToken;

  std::string lastDataPacketSender;

  std::set<std::string> tokenRingHosts;

  std::mt19937 gen{std::random_device{}()};

 private:
  TokenRingPacket createJoinPacket();

  void sendJoinRequest();

  void initializeSockets();

  std::vector<unsigned char> preparePacketRegisterSubheaderFromJoinPacket(
      Socket& incomingSocket, const TokenRingPacket& joinPacket);

  void handleIncomingDataPacket(TokenRingPacket& incomingPacket);

  void handleIncomingJoinPacket(TokenRingPacket& incomingPacket,
                                Socket& incomingSocket);

  void handleIncomingRegisterPacket(TokenRingPacket& incomingPacket);

  template <typename T>
  T random(T min, T max) {
    using dist = std::conditional_t<std::is_integral<T>::value,
                                    std::uniform_int_distribution<T>,
                                    std::uniform_real_distribution<T> >;
    return dist{min, max}(gen);
  }

  template <typename T>
  std::pair<T, bool> getNthElement(std::set<T> & searchSet, int n)
  {
    std::pair<T, bool> result;
    if(searchSet.size() > n )
    {
      result.first = *(std::next(searchSet.begin(), n));
      result.second = true;
    }
    else
      result.second = false;

    return result;
  }

  TokenRingPacket createAndPrepareGreetingsPacketToSend();

public:
  TokenRingDispatcher(ProgramArguments args);

  void run();
};

#endif  // TOKENRINGDISPATCHER_H
