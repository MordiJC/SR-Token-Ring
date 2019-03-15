#include "tokenringudpservice.h"
#include "logger.h"
#include "quitstatusobserver.h"
#include "tokenringpacket.h"
#include "utility.h"

#include <cstring>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

TokenRingUDPService::TokenRingUDPService(
    const ProgramArguments& programArguments)
    : hostId(programArguments.getUserIdentifier()),
      inputSocketPort(programArguments.getPort()),
      nextHostIp(programArguments.getNeighborIp()),
      nextHostPort(programArguments.getNeighborPort()),
      previousHostName(programArguments.getUserIdentifier()),
      tokenStatus(programArguments.getHasToken()) {
  outputSocket = std::make_unique<Socket>(Protocol::UDP);
  inputSocket = std::make_unique<Socket>(Protocol::UDP);
}

void TokenRingUDPService::initializeSockets() {
  inputSocket->bind(Ip4_from_string("127.0.0.1"), inputSocketPort);
  inputSocket->listen(2);
}

void TokenRingUDPService::sendJoinRequestToNextHost() {
  using trppt = TokenRingPacket::PacketType;

  TokenRingPacket joinPacket;

  TokenRingPacket::Header header;

  header.type = trppt::JOIN;
  header.tokenStatus = 1;

  insertStringToCharArrayWithLength(hostId, header.originalSenderName,
                                    TokenRingPacket::NameMaxSize);
  insertStringToCharArrayWithLength(hostId, header.packetSenderName,
                                    TokenRingPacket::NameMaxSize);
  insertStringToCharArrayWithLength("", header.packetReceiverName,
                                    TokenRingPacket::NameMaxSize);

  insertStringToCharArrayWithLength("", header.neighborToDisconnectName,
                                    TokenRingPacket::NameMaxSize);

  header.registerIp = inputSocket->getIp();
  header.registerPort = inputSocket->getPort();

  joinPacket.setHeader(header);
  joinPacket.setData({});

  outputSocket->sendTo(joinPacket.toBinary(), nextHostIp, nextHostPort);
}

void TokenRingUDPService::handleIncomingJoinPacket(
    TokenRingPacket incomingPacket) {
  TokenRingPacket& packet = incomingPacket;

  hosts.insert(packet.getHeader().originalSenderName);

  TokenRingPacket::Header header = packet.getHeader();

  using trppt = TokenRingPacket::PacketType;

  header.type = trppt::REGISTER;
  insertStringToCharArrayWithLength(hostId, header.packetSenderName,
                                    TokenRingPacket::NameMaxSize);
  {
    std::lock_guard<std::mutex> guard(previousHostNameMutex);
    insertStringToCharArrayWithLength(previousHostName,
                                      header.packetReceiverName,
                                      TokenRingPacket::NameMaxSize);
    insertStringToCharArrayWithLength(previousHostName,
                                      header.neighborToDisconnectName,
                                      TokenRingPacket::NameMaxSize);
  }

  packet.setHeader(header);
  packet.setData({});

  {
    std::lock_guard<std::mutex> guard(previousHostNameMutex);
    previousHostName = packet.getHeader().originalSenderName;
  }

  Logger::getInstance().log("[" + hostId + "] Host joining ring: " +
                            packet.getHeader().originalSenderName);

  std::lock_guard<std::mutex> g(registerPacketsMutex);
  registerPackets.push(packet);
}

void TokenRingUDPService::handleIncomingRegisterPacket(
    TokenRingPacket& packet) {
  if (packet.getHeader().tokenStatus) {
    hosts.insert(packet.getHeader().originalSenderName);
    hosts.insert(packet.getHeader().packetSenderName);
    if (packet.getHeader().neighborToDisconnectName == hostId) {
      nextHostIp = packet.getHeader().registerIp;
      nextHostPort = packet.getHeader().registerPort;
      Logger::getInstance().log("[" + hostId + "] Received REGISTER packet.");
    } else {
      if (packet.getHeader().originalSenderName == hostId ||
          (packet.getHeader().neighborToDisconnectName != hostId &&
           packet.getHeader().originalSenderName ==
               std::string(packet.getHeader().neighborToDisconnectName))) {
        Logger::getInstance().log("[" + hostId +
                                  "] Dropping circulating REGISTER packet.");
      } else {
        TokenRingPacket::Header header = packet.getHeader();
        insertStringToCharArrayWithLength(hostId, header.packetSenderName,
                                          TokenRingPacket::NameMaxSize);

        packet.setHeader(header);

        Logger::getInstance().log("[" + hostId +
                                  "] Forwarding REGISTER packet.");

        std::lock_guard<std::mutex> g(registerPacketsMutex);
        registerPackets.push(packet);
      }
    }
    std::lock_guard<std::mutex> lock(tokenStatuCVMutex);
    tokenStatus = true;
    tokenStatusCV.notify_all();

  } else {
    Logger::getInstance().log(
        "[" + hostId + "] Received REGISTER packet has no token. Passing.");
  }
}

void TokenRingUDPService::handleIncomingDataPacket(TokenRingPacket& packet) {
  if (packet.getHeader().tokenStatus) {
    hosts.insert(packet.getHeader().originalSenderName);
    hosts.insert(packet.getHeader().packetSenderName);
    if (packet.getHeader().packetReceiverName == hostId) {
      auto data = packet.getDataAsCharsVector();
      Logger::getInstance().log("[" + hostId +
                                "] Received DATA packet. Contents: \n" +
                                std::string(data.begin(), data.end()));
    } else {
      if (packet.getHeader().originalSenderName == hostId) {
        Logger::getInstance().log("[" + hostId +
                                  "] Dropping circulating DATA packet.");
      } else {
        TokenRingPacket::Header header = packet.getHeader();
        insertStringToCharArrayWithLength(hostId, header.packetSenderName,
                                          TokenRingPacket::NameMaxSize);

        packet.setHeader(header);

        Logger::getInstance().log("[" + hostId + "] Forwarding DATA packet.");

        std::lock_guard<std::mutex> g(dataPacketsMutex);
        dataPackets.push(packet);
      }
    }
    std::lock_guard<std::mutex> lock(tokenStatuCVMutex);
    tokenStatus = true;
    tokenStatusCV.notify_all();

  } else {
    Logger::getInstance().log("[" + hostId +
                              "] Received DATA packet has no token. Passing.");
  }
}

void TokenRingUDPService::senderLoop() {
  TokenRingPacket incomingPacket;
  Serializable::container_type incomingBuffer;

  while (!QuitStatusObserver::getInstance().shouldQuit()) {
    {
      std::unique_lock<std::mutex> lock(tokenStatuCVMutex);
      tokenStatusCV.wait(lock, [this] { return tokenStatus == true; });
    }

    {
      bool done = false;

      std::unique_lock<std::mutex> registerGuard(registerPacketsMutex);
      if (!registerPackets.empty()) {
        TokenRingPacket& packetToSend = registerPackets.front();

        if (packetToSend.getHeader().packetReceiverName != lastReceiverName ||
            packetToSend.getHeader().packetReceiverName == hostId) {
          Logger::getInstance().log("[" + hostId +
                                    "] Sending REGISTER packet.");

          lastReceiverName = packetToSend.getHeader().packetReceiverName;

          outputSocket->sendTo(packetToSend.toBinary(), nextHostIp,
                               nextHostPort);
          registerPackets.pop();

          done = true;

          std::lock_guard<std::mutex> lock(tokenStatuCVMutex);
          tokenStatus = false;
          tokenStatusCV.notify_all();
        }
      }
      registerGuard.unlock();

      if (!done) {
        std::unique_lock<std::mutex> dataGuard(dataPacketsMutex);
        if (!dataPackets.empty()) {
          TokenRingPacket& packetToSend = dataPackets.front();

          if (packetToSend.getHeader().packetReceiverName != lastReceiverName ||
              packetToSend.getHeader().packetReceiverName == hostId) {
            Logger::getInstance().log("[" + hostId + "] Sending DATA packet.");

            lastReceiverName = packetToSend.getHeader().packetReceiverName;

            outputSocket->sendTo(packetToSend.toBinary(), nextHostIp,
                                 nextHostPort);
            dataPackets.pop();

            done = true;

            std::lock_guard<std::mutex> lock(tokenStatuCVMutex);
            tokenStatus = false;
            tokenStatusCV.notify_all();
          }
        }
        dataGuard.unlock();
      }

      if (!done) {
        std::lock_guard<std::mutex> lock(tokenStatuCVMutex);
        tokenStatus = false;
        tokenStatusCV.notify_all();

        TokenRingPacket dataPacket;

        TokenRingPacket::Header header;
        header.type = TokenRingPacket::PacketType::DATA;
        header.tokenStatus = 1;
        insertStringToCharArrayWithLength(hostId, header.originalSenderName,
                                          TokenRingPacket::NameMaxSize);
        insertStringToCharArrayWithLength(hostId, header.packetSenderName,
                                          TokenRingPacket::NameMaxSize);

        std::string packetReceiver = "";
        if (hosts.size() != 0) {
          int idx =
              static_cast<int>(random(0, static_cast<int>(hosts.size() - 1)));

          packetReceiver = getNthElement(hosts, idx).first;
        } else {
          packetReceiver = hostId;
        }
        insertStringToCharArrayWithLength(packetReceiver,
                                          header.packetReceiverName,
                                          TokenRingPacket::NameMaxSize);

        dataPacket.setHeader(header);

        std::string message = "Greetings from " + hostId;
        std::vector<unsigned char> messageBuffer;
        messageBuffer.resize(message.size());

        std::memcpy(messageBuffer.data(), message.c_str(), message.size());

        dataPacket.setData(messageBuffer);

        Logger::getInstance().log("[" + hostId +
                                  "] Sending greetings packet to `" +
                                  packetReceiver + "`");

        outputSocket->sendTo(dataPacket.toBinary(), nextHostIp, nextHostPort);

        std::this_thread::sleep_for(std::chrono::seconds{5});
      }
    }
  }
}

void TokenRingUDPService::run() {
  initializeSockets();

  sendJoinRequestToNextHost();

  TokenRingPacket incomingPacket;
  Serializable::container_type incomingBuffer;
  Ip4 incomingIp;
  unsigned short incomingPort;

  std::thread senderThreadService{&TokenRingUDPService::senderLoop, this};

  while (!QuitStatusObserver::getInstance().shouldQuit()) {
    try {
      auto result = inputSocket->receiveFrom(sizeof(TokenRingPacket::Header) +
                                             TokenRingPacket::DataMaxSize);

      incomingIp = result.first.first;
      incomingPort = result.first.second;
      incomingBuffer = result.second;

    } catch (const SocketReceivingFailedException& ex) {
      Logger::getInstance().log("[" + hostId +
                                "] Packet receiving failed: " + ex.what());
      continue;
    }

    try {
      size_t extractedBytes = incomingPacket.fromBinary(incomingBuffer);
      (void)extractedBytes;

    } catch (const TokenRingPacketException& ex) {
      Logger::getInstance().log(
          "[" + hostId + "] TokenRingPacket creation failed: " + ex.what());
      continue;
    }

    using trppt = TokenRingPacket::PacketType;

    switch (incomingPacket.getHeader().type) {
      case trppt::JOIN:
        // Handle JOIN PACKET
        handleIncomingJoinPacket(incomingPacket);
        break;
      case trppt::REGISTER:
        // Handle REGISTER PACKET
        handleIncomingRegisterPacket(incomingPacket);
        break;
      case trppt::DATA:
        // Handle DATA PACKET
        handleIncomingDataPacket(incomingPacket);
        break;
      default:
        Logger::getInstance().log("[" + hostId +
                                  "] Packet with unknown type received.");
    }
    std::lock_guard<std::mutex> lock(tokenStatuCVMutex);
    tokenStatusCV.notify_all();
  }
}
