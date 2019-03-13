#include "tokenringdispatcher.h"
#include "quitstatusobserver.h"

#include <algorithm>
#include <cstring>
#include <iostream>
#include <thread>

#include "utility.h"

const std::string TokenRingDispatcher::workingIp{"127.0.0.1"};

TokenRingPacket TokenRingDispatcher::createJoinPacket() {
  std::cout << "[INFO] Creating JOIN packet" << std::endl;
  TokenRingPacket packet;
  TokenRingPacket::Header packetHeader;
  packetHeader.type = TokenRingPacket::PacketType::JOIN;
  packetHeader.tokenStatus = 1;
  insertStringToCharArrayWithLength(programArguments.getUserIdentifier(),
                                    packetHeader.packetSenderName,
                                    TokenRingPacket::UserIdentifierNameSize);
  insertStringToCharArrayWithLength(programArguments.getUserIdentifier(),
                                    packetHeader.originalSenderName,
                                    TokenRingPacket::UserIdentifierNameSize);
  insertStringToCharArrayWithLength("", packetHeader.packetReceiverName,
                                    TokenRingPacket::UserIdentifierNameSize);

  packetHeader.registerIp = Ip4_from_string(workingIp);
  packetHeader.registerPort = programArguments.getPort();
  insertStringToCharArrayWithLength("", packetHeader.neighborToDisconnectName,
                                    TokenRingPacket::UserIdentifierNameSize);

  packet.setHeader(packetHeader);
  packet.setData({});

  const TokenRingPacket::Header& packetHeaderRef = packet.getHeader();

  std::cout << "JOIN PACKET HEADER:" << std::endl
            << "Type: JOIN" << std::endl
            << "TokenStatus: Available" << std::endl
            << "PacketSender: " << packetHeaderRef.packetSenderName << std::endl
            << "OriginalSender: " << packetHeaderRef.originalSenderName
            << std::endl
            << "PacketReceiver: " << packetHeaderRef.packetReceiverName
            << std::endl
            << "DataSize: " << packetHeaderRef.dataSize << std::endl
            << "RegisterIP: " << to_string(packetHeaderRef.registerIp)
            << std::endl
            << "RegisterPort: " << packetHeaderRef.registerPort << std::endl
            << "NeighborToDisconnect: "
            << packetHeaderRef.neighborToDisconnectName << std::endl;

  return packet;
}

TokenRingDispatcher::TokenRingDispatcher(ProgramArguments args)
    : programArguments(args), hasToken(args.getHasToken()) {
  outputSocket = std::make_unique<Socket>(args.getProtocol());
  neighborSocket = std::make_unique<Socket>(args.getProtocol());
}

void TokenRingDispatcher::sendJoinRequest() {
  std::cout << "[INFO] Sending JOIN request" << std::endl;
  TokenRingPacket joinPacket = createJoinPacket();

  outputSocket->send(joinPacket.toBinary());
}

void TokenRingDispatcher::initializeSockets() {
  Ip4 workingIp4 = Ip4_from_string(workingIp);
  std::cout << "[INFO] Input socket binding to " << to_string(workingIp4) << ':'
            << programArguments.getPort() << std::endl;
  neighborSocket->bind(workingIp4, programArguments.getPort());
  std::cout << "[INFO] Input socket bound to " << to_string(workingIp4) << ':'
            << programArguments.getPort() << std::endl;

  neighborSocket->listen(2);
  std::cout << "[INFO] Input socket listening with backlog of 2" << std::endl;

  outputSocket->connect(programArguments.getNeighborIp(),
                        programArguments.getNeighborPort());

  std::cout << "[INFO] Output socket connected "
            << to_string(outputSocket->getIp()) << ':'
            << outputSocket->getPort() << " -> "
            << to_string(programArguments.getNeighborIp()) << ':'
            << programArguments.getNeighborPort() << std::endl;
}

void TokenRingDispatcher::preparePacketRegisterSubheaderFromJoinPacket(
    Socket& incomingSocket, TokenRingPacket::Header& header) {
  insertStringToCharArrayWithLength(programArguments.getUserIdentifier(),
                                    header.neighborToDisconnectName,
                                    TokenRingPacket::UserIdentifierNameSize);
  header.registerIp = incomingSocket.getConnectionIp();
  header.registerPort = incomingSocket.getConnectionPort();
}

void TokenRingDispatcher::handleIncomingDataPacket(
    TokenRingPacket& incomingPacket) {
  if (incomingPacket.getHeader().tokenStatus) {
    if (incomingPacket.getHeader().packetReceiverName ==
        programArguments.getUserIdentifier()) {
      std::string::size_type strSize = incomingPacket.getHeader().dataSize;

      std::string message{incomingPacket.getDataAsSignedCharsVector().data(), strSize};

      std::cout << "[Host: @" << programArguments.getUserIdentifier()
                << "] Received data: `" << message << "\'"
                << std::endl;
    } else {
      TokenRingPacket::Header header = incomingPacket.getHeader();
      insertStringToCharArrayWithLength(
          programArguments.getUserIdentifier(), header.packetSenderName,
          TokenRingPacket::UserIdentifierNameSize);
      insertStringToCharArrayWithLength(
          programArguments.getUserIdentifier(), header.packetSenderName,
          TokenRingPacket::UserIdentifierNameSize);

      incomingPacket.setHeader(header);
      dataPackets.push(incomingPacket);
    }

    hasToken = true;
  }
  std::cout << "[INFO] Received DATA packet withoud token." << std::endl;
}

void TokenRingDispatcher::handleIncomingJoinPacket(
    TokenRingPacket& incomingPacket, Socket& incomingSocket) {
  using trppt = TokenRingPacket::PacketType;

  const TokenRingPacket::Header& packetHeaderRef = incomingPacket.getHeader();

  std::cout << "INCOMING JOIN PACKET HEADER:" << std::endl
            << "Type: JOIN" << std::endl
            << "TokenStatus: Available" << std::endl
            << "PacketSender: " << packetHeaderRef.packetSenderName << std::endl
            << "OriginalSender: " << packetHeaderRef.originalSenderName
            << std::endl
            << "PacketReceiver: " << packetHeaderRef.packetReceiverName
            << std::endl
            << "DataSize: " << packetHeaderRef.dataSize << std::endl
            << "RegisterIP: " << to_string(packetHeaderRef.registerIp)
            << std::endl
            << "RegisterPort: " << packetHeaderRef.registerPort << std::endl
            << "NeighborToDisconnect: "
            << packetHeaderRef.neighborToDisconnectName << std::endl;

  if (incomingPacket.getHeader().originalSenderName ==
      programArguments.getUserIdentifier()) {
    std::cout << "[INFO] Adding self to token ring hosts list" << std::endl;

    tokenRingHosts.insert(incomingPacket.getHeader().originalSenderName);

    neighborIp = packetHeaderRef.registerIp;
    neighborPort = packetHeaderRef.registerPort;

  } else {
    std::cout << "[INFO] Preparing REGISTER header" << std::endl;
    TokenRingPacket::Header header = incomingPacket.getHeader();
    header.type = trppt::REGISTER;
    preparePacketRegisterSubheaderFromJoinPacket(incomingSocket, header);

    incomingPacket.setHeader(header);

    registerRequestPackets.push(incomingPacket);
  }
}

TokenRingPacket TokenRingDispatcher::createAndPrepareGreetingsPacketToSend() {
  using trppt = TokenRingPacket::PacketType;

  TokenRingPacket packet;

  TokenRingPacket::Header header;
  std::memset(&header, 0, sizeof(header));
  header.type = trppt::DATA;
  header.tokenStatus = 1;
  insertStringToCharArrayWithLength(programArguments.getUserIdentifier(),
                                    header.originalSenderName,
                                    TokenRingPacket::UserIdentifierNameSize);
  insertStringToCharArrayWithLength(programArguments.getUserIdentifier(),
                                    header.packetSenderName,
                                    TokenRingPacket::UserIdentifierNameSize);

  int idx = static_cast<int>(random(0UL, tokenRingHosts.size() - 1));
  std::string packetReceiver = getNthElement(tokenRingHosts, idx).first;
  insertStringToCharArrayWithLength(packetReceiver, header.packetReceiverName,
                                    TokenRingPacket::UserIdentifierNameSize);

  packet.setHeader(header);

  std::string message =
      std::string("Greetings from ") + programArguments.getUserIdentifier();

  std::vector<Serializable::data_type> messageBuffer;
  messageBuffer.resize(message.size());

  std::memcpy(messageBuffer.data(), message.data(), message.size());

  packet.setData(messageBuffer);

  return packet;
}

void TokenRingDispatcher::handleIncomingRegisterPacket(
    TokenRingPacket& incomingPacket) {
  std::cout << "[INFO] Handling incoming REGISTER packet" << std::endl;

  tokenRingHosts.insert(incomingPacket.getHeader().originalSenderName);

  if (incomingPacket.getHeader().neighborToDisconnectName == neighborName) {
    //    outputSocket->disconnect();
    outputSocket->close();
    delete outputSocket.release();
    outputSocket = std::make_unique<Socket>(programArguments.getProtocol());
    outputSocket->connect(incomingPacket.getHeader().registerIp,
                          incomingPacket.getHeader().registerPort);
    neighborIp = incomingPacket.getHeader().registerIp;
    neighborPort = incomingPacket.getHeader().registerPort;
    std::cout << "[INFO][CONNECTING] " << to_string(outputSocket->getIp())
              << ':' << outputSocket->getPort() << " to "
              << to_string(outputSocket->getConnectionIp()) << ':'
              << outputSocket->getConnectionPort() << std::endl;
  } else {
    registerRequestPackets.push(incomingPacket);
  }
}

void TokenRingDispatcher::run() {
  initializeSockets();

  sendJoinRequest();

  TokenRingPacket incomingPacket;
  Serializable::container_type incomingBuffer;

  Socket incomingSocket{neighborSocket->select(std::chrono::seconds{5})};

  std::cout << "[INFO] Select passed positive" << std::endl;

  while (!QuitStatusObserver::getInstance().shouldQuit()) {
    try {
      try {
        if (programArguments.getProtocol() == Protocol::TCP) {
          std::cout << "[INFO] Receiving from: "
                    << to_string(incomingSocket.getConnectionIp()) << ':'
                    << incomingSocket.getConnectionPort() << std::endl;

          incomingBuffer = incomingSocket.receive();

          std::cout << "[INFO] Received packet from "
                    << to_string(incomingSocket.getConnectionIp()) << ':'
                    << incomingSocket.getConnectionPort()
                    << " with size of size: " << incomingBuffer.size()
                    << std::endl;
        } else {
          incomingBuffer = incomingSocket.receiveFrom().second;
        }

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

      bool shouldPass = false;

      switch (incomingPacket.getHeader().type) {
        case trppt::DATA:
          std::cout << "[INFO] Handling incoming DATA packet" << std::endl;
          handleIncomingDataPacket(incomingPacket);
          break;

        case trppt::JOIN:
          std::cout << "[INFO] Handling incoming JOIN packet" << std::endl;
          handleIncomingJoinPacket(incomingPacket, incomingSocket);
          break;
        case trppt::REGISTER: {
          try {
            handleIncomingRegisterPacket(incomingPacket);
          } catch (
              const TokenRingDispatcherNotEnoughDataForRegisterSubheaderException&
                  ex) {
            std::cerr << "[ERROR] " << ex.what() << std::endl;
          }
        } break;
        default:
          std::cerr << "[ERROR] Unknown packet type. Packet type code: "
                    << static_cast<std::underlying_type_t<trppt>>(
                           incomingPacket.getHeader().type)
                    << std::endl;
          shouldPass = true;
      }

      if (shouldPass) {
        continue;
      }

    } catch (const SocketSelectTimeoutException& ex) {
      std::cout << "[INFO] " << ex.what() << std::endl;
    }

    if (!hasToken) {
      std::cout << "[INFO] No token, passing." << std::endl;
      continue;
    }

    // Handle packets from queues and send them
    if (!registerRequestPackets.empty()) {
      TokenRingPacket& packetToSend = registerRequestPackets.front();
      registerRequestPackets.pop();

      outputSocket->send(packetToSend.toBinary());
      hasToken = false;
    } else if (!dataPackets.empty()) {
      TokenRingPacket& packetToSend = dataPackets.front();

      if (packetToSend.getHeader().originalSenderName == lastDataPacketSender) {
        std::cout << "[INFO] Sending greetings packet (1-1)" << std::endl;
        TokenRingPacket greetingsPacket =
            createAndPrepareGreetingsPacketToSend();
        lastDataPacketSender = greetingsPacket.getHeader().originalSenderName;
        outputSocket->send(greetingsPacket.toBinary());
        std::this_thread::sleep_for(std::chrono::seconds{2});
      } else {
        std::cout << "[INFO] Sending data packet to next node" << std::endl;
        dataPackets.pop();
        lastDataPacketSender = packetToSend.getHeader().originalSenderName;
        outputSocket->send(packetToSend.toBinary());
      }
      hasToken = false;
    } else {
      std::cout << "[INFO] Sending greetings packet (2-1)" << std::endl;
      std::cout << "[INFO] Sending from " << to_string(outputSocket->getIp())
                << ':' << outputSocket->getPort() << " to "
                << to_string(outputSocket->getConnectionIp()) << ':'
                << outputSocket->getConnectionPort() << std::endl;

      TokenRingPacket packetToSend = createAndPrepareGreetingsPacketToSend();
      lastDataPacketSender = packetToSend.getHeader().originalSenderName;
      outputSocket->send(packetToSend.toBinary());
      std::this_thread::sleep_for(std::chrono::seconds{2});
      hasToken = false;
    }
  }
}
