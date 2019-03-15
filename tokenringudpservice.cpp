#include "tokenringudpservice.h"
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

  std::cout << "[INFO] Handled JOIN packet:" << std::endl << packet.to_string();

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
    } else {
      if (packet.getHeader().originalSenderName == hostId ||
          (packet.getHeader().neighborToDisconnectName != hostId &&
           packet.getHeader().originalSenderName ==
               std::string(packet.getHeader().neighborToDisconnectName))) {
        std::cout << "[INFO] Dropping circulating REGISTER packet."
                  << std::endl;
      } else {
        TokenRingPacket::Header header = packet.getHeader();
        insertStringToCharArrayWithLength(hostId, header.packetSenderName,
                                          TokenRingPacket::NameMaxSize);

        packet.setHeader(header);

        std::cout << "[INFO] Forwarding REGISTER packet" << std::endl
                  << packet.to_string() << std::endl;

        std::lock_guard<std::mutex> g(registerPacketsMutex);
        registerPackets.push(packet);
      }
    }
    std::lock_guard<std::mutex> lock(tokenStatuCVMutex);
    tokenStatus = true;
    tokenStatusCV.notify_all();

    std::cout << "[INFO][TOKEN] Token granted" << std::endl;

  } else {
    std::cout << "[ERROR] Received REGISTER packet has no token. Passing."
              << std::endl;
  }
}

void TokenRingUDPService::handleIncomingDataPacket(TokenRingPacket& packet) {
  if (packet.getHeader().tokenStatus) {
    hosts.insert(packet.getHeader().originalSenderName);
    hosts.insert(packet.getHeader().packetSenderName);
    if (packet.getHeader().packetReceiverName == hostId) {
      auto data = packet.getDataAsCharsVector();
      std::cout << "[INFO] DATA packet received!" << std::endl
                << "CONTENTS: " << std::string(data.begin(), data.end())
                << std::endl;
    } else {
      if (packet.getHeader().originalSenderName == hostId) {
        std::cout << "[INFO] Dropping circulating DATA packet." << std::endl
                  << packet.to_string() << std::endl;
      } else {
        TokenRingPacket::Header header = packet.getHeader();
        insertStringToCharArrayWithLength(hostId, header.packetSenderName,
                                          TokenRingPacket::NameMaxSize);

        packet.setHeader(header);

        std::cout << "[INFO] Forwarding DATA packet:" << std::endl
                  << packet.to_string() << std::endl;

        std::lock_guard<std::mutex> g(dataPacketsMutex);
        dataPackets.push(packet);
      }
    }
    std::lock_guard<std::mutex> lock(tokenStatuCVMutex);
    tokenStatus = true;
    tokenStatusCV.notify_all();

    std::cout << "[INFO][TOKEN] Token granted" << std::endl;

  } else {
    std::cout << "[ERROR] Received DATA packet has no token. Passing."
              << std::endl;
  }
}

void TokenRingUDPService::senderLoop() {
  TokenRingPacket incomingPacket;
  Serializable::container_type incomingBuffer;

  std::cout << "[INFO] Sender Loop thread starting" << std::endl;

  while (!QuitStatusObserver::getInstance().shouldQuit()) {
    std::cout << "[INFO][SENDER] Waiting for token" << std::endl;
    {
      std::unique_lock<std::mutex> lock(tokenStatuCVMutex);
      tokenStatusCV.wait(lock, [this] { return tokenStatus == true; });
    }

    std::cout << "[INFO][SENDER] Token acquired" << std::endl;
    {
      bool done = false;

      std::unique_lock<std::mutex> registerGuard(registerPacketsMutex);
      if (!registerPackets.empty()) {
        TokenRingPacket& packetToSend = registerPackets.front();
        std::cout << "[INFO][SENDER] Sending REGISTER packet" << std::endl
                  << packetToSend.to_string() << std::endl;

        outputSocket->sendTo(packetToSend.toBinary(), nextHostIp, nextHostPort);
        registerPackets.pop();

        done = true;

        std::lock_guard<std::mutex> lock(tokenStatuCVMutex);
        tokenStatus = false;
        tokenStatusCV.notify_all();
      }
      registerGuard.unlock();

      if (!done) {
        std::unique_lock<std::mutex> dataGuard(dataPacketsMutex);
        if (!dataPackets.empty()) {
          TokenRingPacket& packetToSend = dataPackets.front();

          std::cout << "[INFO][SENDER] Sending DATA packet" << std::endl
                    << packetToSend.to_string() << std::endl;

          outputSocket->sendTo(packetToSend.toBinary(), nextHostIp,
                               nextHostPort);
          dataPackets.pop();

          done = true;

          std::lock_guard<std::mutex> lock(tokenStatuCVMutex);
          tokenStatus = false;
          tokenStatusCV.notify_all();
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

        std::cout << "[INFO][SENDER] Sending Greetings packet to "
                  << packetReceiver << std::endl;

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

      std::cout << "[INFO] Received packet from " << to_string(incomingIp)
                << ':' << incomingPort
                << " with size of size: " << incomingBuffer.size() << std::endl;
    } catch (const SocketReceivingFailedException& ex) {
      std::cerr << "[ERROR] Socket receiving failed: " << ex.what()
                << std::endl;
      continue;
    }

    try {
      size_t extractedBytes = incomingPacket.fromBinary(incomingBuffer);
      std::cout << "[INFO] Extracted bytes: " << extractedBytes << std::endl;

    } catch (const TokenRingPacketException& ex) {
      std::cerr << "[ERROR] Token Ring Packet creation failed: " << ex.what()
                << std::endl;
      continue;
    }

    using trppt = TokenRingPacket::PacketType;

    std::cout << "[INFO] Processing packet" << std::endl
              << incomingPacket.to_string() << std::endl;

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
        std::cout << "[ERROR] Unknown packet type" << std::endl
                  << incomingPacket.to_string() << std::endl;
    }
    std::cout << "[INFO][TOKEN] Notifying about token status: "
              << std::boolalpha << tokenStatus << std::endl;
    std::lock_guard<std::mutex> lock(tokenStatuCVMutex);
    tokenStatusCV.notify_all();
  }
}
