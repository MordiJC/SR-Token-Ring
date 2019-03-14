#ifndef TOKENRINGDISPATCHER_H
#define TOKENRINGDISPATCHER_H

#include <atomic>
#include <memory>
#include <mutex>
#include <queue>
#include <random>
#include <set>
#include <stdexcept>
#include <thread>

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

  std::recursive_mutex registerRequestPacketsMutex;
  std::recursive_mutex dataPacketsMutex;

  std::unique_ptr<Socket> outputSocket;
  std::unique_ptr<Socket> inputSocket;

  std::string previousHostName;

  Ip4 neighborIp;
  unsigned short neighborPort = 0;

  ProgramArguments programArguments;

  bool hasToken;

  std::string lastDataPacketSender;

  std::set<std::string> tokenRingHosts;

  std::mt19937 gen{std::random_device{}()};

  std::unique_ptr<std::thread> tokenRingInputProcessingThread{nullptr};

  std::atomic_bool tokenRingInputProcessingThreadDone{false};

  std::unique_ptr<Socket> tokenRingInputProcessingThreadSocketHolder{nullptr};

 private:
  void initializeSockets();

  TokenRingPacket createJoinPacket();

  void sendJoinRequest();

  void preparePacketRegisterSubheaderFromJoinPacket(
      Socket& incomingSocket, TokenRingPacket::Header& header);

  void handleDataPacket(TokenRingPacket& incomingPacket);

  void handleJoinPacket(TokenRingPacket& incomingPacket,
                                Socket& incomingSocket);

  void handleRegisterPacket(TokenRingPacket& incomingPacket);

  template <typename T>
  T random(T min, T max) {
    using dist = std::conditional_t<std::is_integral<T>::value,
                                    std::uniform_int_distribution<T>,
                                    std::uniform_real_distribution<T> >;
    return dist{min, max}(gen);
  }

  template <typename T>
  std::pair<T, bool> getNthElement(std::set<T>& searchSet, int n) {
    std::pair<T, bool> result;
    if (searchSet.size() > n) {
      result.first = *(std::next(searchSet.begin(), n));
      result.second = true;
    } else
      result.second = false;

    return result;
  }

  TokenRingPacket createAndPrepareGreetingsPacketToSend();

  void processNeighborSocket(Socket neighborSocket, std::atomic_bool& done);

 public:
  TokenRingDispatcher(ProgramArguments args);

  void run();
};

#endif  // TOKENRINGDISPATCHER_H
