#ifndef TOKENRINGUDPSERVICE_H
#define TOKENRINGUDPSERVICE_H

#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <queue>
#include <set>
#include <string>

#include "ip4.h"
#include "programarguments.h"
#include "socket.h"
#include "tokenringpacket.h"

class TokenRingUDPService {
  // Private variables
 private:
  std::unique_ptr<Socket> outputSocket;
  std::unique_ptr<Socket> inputSocket;

  std::string hostId;
  unsigned short inputSocketPort;

  std::atomic<Ip4> nextHostIp;
  std::atomic<unsigned short> nextHostPort;

  std::mutex previousHostNameMutex;
  std::string previousHostName;

  bool tokenStatus{false};

  std::set<std::string> hosts;

  std::mutex registerPacketsMutex;
  std::queue<TokenRingPacket> registerPackets;

  std::mutex dataPacketsMutex;
  std::queue<TokenRingPacket> dataPackets;

  std::mutex tokenStatuCVMutex;
  std::condition_variable tokenStatusCV;

  // Private methods
 private:
  void initializeSockets();

  void sendJoinRequestToNextHost();

  void handleIncomingJoinPacket(TokenRingPacket incomingPacket);

  void handleIncomingRegisterPacket(TokenRingPacket& packet);

  void handleIncomingDataPacket(TokenRingPacket & packet);

  void senderLoop();

public:
  TokenRingUDPService(const ProgramArguments &programArguments);

  void run() noexcept(false);
};

#endif  // TOKENRINGUDPSERVICE_H
