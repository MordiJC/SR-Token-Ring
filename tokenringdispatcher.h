#ifndef TOKENRINGDISPATCHER_H
#define TOKENRINGDISPATCHER_H

#include <memory>
#include <queue>
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

  ProgramArguments programArguments;

  bool hasToken;

  TokenRingPacket createJoinPacket();

  void sendJoinRequest();

  void initializeSockets();

  std::vector<unsigned char> preparePacketRegisterSubheaderFromJoinPacket(
      Socket& incomingSocket, const TokenRingPacket& joinPacket);

  void handleIncomingDataPacket(TokenRingPacket& incomingPacket);

  void handleIncomingJoinPacket(TokenRingPacket& incomingPacket,
                                Socket& incomingSocket);

 public:
  TokenRingDispatcher(ProgramArguments args);

  void run();
};

#endif  // TOKENRINGDISPATCHER_H
