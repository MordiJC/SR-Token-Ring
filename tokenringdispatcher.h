#ifndef TOKENRINGDISPATCHER_H
#define TOKENRINGDISPATCHER_H

#include <memory>
#include <queue>

#include "programarguments.h"
#include "socket.h"
#include "tokenringpacket.h"

class TokenRingDispatcher {
 private:
  std::queue<TokenRingPacket> registerRequestPackets;  // First priority
  std::queue<TokenRingPacket> dataPackets;             // Second priority

  std::unique_ptr<Socket> outputSocket;
  std::unique_ptr<Socket> neighborSocket;

  std::string neighborName;

  ProgramArguments programArguments;

  TokenRingPacket createJoinPacket();

 public:
  TokenRingDispatcher(ProgramArguments args);

  void run();
};

#endif  // TOKENRINGDISPATCHER_H
