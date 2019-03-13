#include "tokenringdispatcher.h"
#include "quitstatusobserver.h"

#include <algorithm>
#include <cstring>
#include <iostream>

const std::string TokenRingDispatcher::workingIp{"127.0.0.1"};

TokenRingPacket TokenRingDispatcher::createJoinPacket() {
  std::cout << "[INFO] Creating JOIN packet" << std::endl;
  TokenRingPacket packet;

  TokenRingPacket::Header packetHeader;
  packetHeader.type = TokenRingPacket::PacketType::JOIN;
  packetHeader.tokenStatus = 1;
  packetHeader.setPacketSenderName(programArguments.getUserIdentifier());
  packetHeader.setOriginalSenderName(programArguments.getUserIdentifier());
  packetHeader.setPacketReceiverName("");

  TokenRingPacket::RegisterSubheader registerSubheader;
  registerSubheader.ip = Ip4(workingIp);
  registerSubheader.port = programArguments.getPort();
  registerSubheader.setNeighborToDisconnectName("");

  std::vector<unsigned char> data = registerSubheader.toBinary();

  packet.setHeader(packetHeader);
  packet.setData(data);

  std::cout << "JOIN PACKET HEADER:" << std::endl
            << "Type: JOIN" << std::endl
            << "TokenStatus: Available" << std::endl
            << "PacketSender: " << packetHeader.packetSenderNameToString()
            << std::endl
            << "OriginalSender: " << packetHeader.originalSenderNameToString()
            << std::endl
            << "PacketReceiver: " << packetHeader.packetReceiverNameToString()
            << std::endl
            << "DataSize: " << packetHeader.dataSize << std::endl
            << "JOIN PACKET SUBHEADER:" << std::endl
            << "IP: " << registerSubheader.ip.to_string() << std::endl
            << "Port: " << registerSubheader.port << std::endl
            << "NeighborToDisconnect: "
            << registerSubheader.neighborToDisconnectNameToString()
            << std::endl;

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
  Ip4 workingIp4{workingIp};
  neighborSocket->bind(workingIp4, programArguments.getPort());
  std::cout << "[INFO] Input socket bound to " << workingIp4.to_string() << ':'
            << programArguments.getPort() << std::endl;

  neighborSocket->listen(2);
  std::cout << "[INFO] Input socket listening with backlog of 2" << std::endl;

  outputSocket->connect(programArguments.getNeighborIp(),
                        programArguments.getNeighborPort());

  std::cout << "[INFO] Output socket connected "
            << outputSocket->getIp().to_string() << ':'
            << outputSocket->getPort() << " -> "
            << programArguments.getNeighborIp().to_string() << ':'
            << programArguments.getNeighborPort() << std::endl;
}

std::vector<unsigned char>
TokenRingDispatcher::preparePacketRegisterSubheaderFromJoinPacket(
    Socket& incomingSocket, const TokenRingPacket& joinPacket) {
  TokenRingPacket::RegisterSubheader registerSubheader;

  registerSubheader.fromBinary(joinPacket.getData());

  registerSubheader.setNeighborToDisconnectName(
      programArguments.getUserIdentifier());
  registerSubheader.ip = incomingSocket.getConnectionIp();
  registerSubheader.port = incomingSocket.getConnectionPort();

  return registerSubheader.toBinary();
}

void TokenRingDispatcher::handleIncomingDataPacket(
    TokenRingPacket& incomingPacket) {
  if (incomingPacket.getHeader().tokenStatus) {
    TokenRingPacket::Header header = incomingPacket.getHeader();
    header.setPacketSenderName(programArguments.getUserIdentifier());
    if (incomingPacket.getHeader().packetReceiverName ==
        programArguments.getUserIdentifier()) {
      std::cout << "[Host: @" << programArguments.getUserIdentifier()
                << "] Received data: `"
                << std::string(incomingPacket.getData().begin(),
                               incomingPacket.getData().end())
                << "\'" << std::endl;
      header.setOriginalSenderName(programArguments.getUserIdentifier());
      header.setPacketReceiverName(neighborName);
    }
    // else:
    incomingPacket.setHeader(header);

    incomingPacket.setData({});
    hasToken = true;
    dataPackets.push(incomingPacket);
  }
}

void TokenRingDispatcher::handleIncomingJoinPacket(
    TokenRingPacket& incomingPacket, Socket& incomingSocket) {
  using trppt = TokenRingPacket::PacketType;

  if (incomingPacket.getHeader().originalSenderNameToString() ==
      programArguments.getUserIdentifier()) {
    std::cout << "[INFO] Adding self to token ring hosts list" << std::endl;
    TokenRingPacket::RegisterSubheader registerSubheader;

    if (incomingPacket.getHeader().dataSize < TokenRingPacket::RegisterSubheader::SIZE) {
      throw TokenRingDispatcherNotEnoughDataForRegisterSubheaderException(
          "Incoming packet has not enough data to construct Register "
          "Subheader");
    }

    registerSubheader.fromBinary(incomingPacket.getData());

    tokenRingHosts.insert(
        incomingPacket.getHeader().originalSenderNameToString());


    outputSocket->disconnect();
    outputSocket->close();
    delete outputSocket.release();
    outputSocket = std::make_unique<Socket>(programArguments.getProtocol());
    outputSocket->connect(registerSubheader.ip, registerSubheader.port);

    neighborIp = registerSubheader.ip;
    neighborPort = registerSubheader.port;
  } else {
    TokenRingPacket::Header header = incomingPacket.getHeader();
    header.type = trppt::REGISTER;
    incomingPacket.setHeader(header);
    std::vector<unsigned char> registerSubheaderBinaryForm =
        preparePacketRegisterSubheaderFromJoinPacket(incomingSocket,
                                                     incomingPacket);

    incomingPacket.setData(registerSubheaderBinaryForm);

    registerRequestPackets.push(incomingPacket);
  }
  /* else {
    std::cerr << "[ERROR] Invalid state! Terminating!" << std::endl;
    QuitStatusObserver::getInstance().quit();
  }*/
}

TokenRingPacket TokenRingDispatcher::createAndPrepareGreetingsPacketToSend() {
  using trppt = TokenRingPacket::PacketType;

  TokenRingPacket packet;

  TokenRingPacket::Header header;
  header.type = trppt::DATA;
  header.tokenStatus = 1;
  header.setOriginalSenderName(programArguments.getUserIdentifier());
  header.setPacketSenderName(programArguments.getUserIdentifier());
  int idx = static_cast<int>(random(0UL, tokenRingHosts.size() - 1));
  std::string packetReceiver = getNthElement(tokenRingHosts, idx).first;
  header.setPacketReceiverName(packetReceiver);

  packet.setHeader(header);

  std::string message =
      "Greetings from " + programArguments.getUserIdentifier();

  std::vector<Serializable::data_type> messageBuffer;
  messageBuffer.resize(message.size() + 1);

  std::memcpy(messageBuffer.data(), message.c_str(), message.size() + 1);

  packet.setData(messageBuffer);

  return packet;
}

void TokenRingDispatcher::handleIncomingRegisterPacket(
    TokenRingPacket& incomingPacket) {
  std::cout << "[INFO] Handling incoming REGISTER packet" << std::endl;
  TokenRingPacket::RegisterSubheader registerSubheader;

  if (incomingPacket.getHeader().dataSize < TokenRingPacket::RegisterSubheader::SIZE) {
    throw TokenRingDispatcherNotEnoughDataForRegisterSubheaderException(
        "Incoming packet has not enough data to construct Register Subheader");
  }

  tokenRingHosts.insert(
      incomingPacket.getHeader().originalSenderNameToString());

  registerSubheader.fromBinary(incomingPacket.getData());

  if (registerSubheader.neighborToDisconnectNameToString() == neighborName) {
    // TODO: Disconnect next neighbor and connect new
    outputSocket->disconnect();
    outputSocket->close();
    delete outputSocket.release();
    outputSocket = std::make_unique<Socket>(programArguments.getProtocol());
    outputSocket->connect(registerSubheader.ip, registerSubheader.port);
    neighborIp = registerSubheader.ip;
    neighborPort = registerSubheader.port;
  } else {
    registerRequestPackets.push(incomingPacket);
  }
}

void TokenRingDispatcher::run() {
  initializeSockets();

  sendJoinRequest();

  TokenRingPacket incomingPacket;
  Serializable::container_type incomingBuffer;

  while (!QuitStatusObserver::getInstance().shouldQuit()) {
    std::cout << "Any data to read: " << std::boolalpha
              << neighborSocket->hasAnyDataToRead() << std::endl;
    try {
      Socket incomingSocket = neighborSocket->select(std::chrono::seconds{5});
      // Socket incomingSocket = neighborSocket->accept();

      std::cout << "[INFO] Select passed positive" << std::endl;

      try {
        if (programArguments.getProtocol() == Protocol::TCP) {
          incomingBuffer = incomingSocket.receive();
          std::cout << "[INFO] Received packet size: " << incomingBuffer.size() << std::endl;
        } else {
          incomingBuffer = incomingSocket.receiveFrom().second;
        }

      } catch (const SocketReceivingFailedException& ex) {
        std::cerr << "[ERROR] Socket receiving failed: " << ex.what()
                  << std::endl;
        continue;
      }

      try {
        incomingPacket.fromBinary(incomingBuffer);
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
          shouldPass = true;
          break;
        case trppt::REGISTER: {
          try {
            handleIncomingRegisterPacket(incomingPacket);
          } catch (
              const TokenRingDispatcherNotEnoughDataForRegisterSubheaderException&
                  ex) {
            std::cerr << "[ERROR] " << ex.what() << std::endl;
            shouldPass = true;
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
      TokenRingPacket packetToSend = registerRequestPackets.front();
      registerRequestPackets.pop();

      outputSocket->send(packetToSend.toBinary());
    } else if (!dataPackets.empty()) {
      TokenRingPacket packetToSend = dataPackets.front();

      if (packetToSend.getHeader().originalSenderNameToString() ==
          lastDataPacketSender) {
        std::cout << "[INFO] Sending greetings packet (1-1)" << std::endl;
        packetToSend = createAndPrepareGreetingsPacketToSend();

      } else {
        std::cout << "[INFO] Sending data packet to next node" << std::endl;
        dataPackets.pop();
      }

      lastDataPacketSender =
          packetToSend.getHeader().originalSenderNameToString();
      outputSocket->send(packetToSend.toBinary());
      hasToken = false;
    } else {
      std::cout << "[INFO] Sending greetings packet (2-1)" << std::endl;
      TokenRingPacket packetToSend = createAndPrepareGreetingsPacketToSend();
      lastDataPacketSender =
          packetToSend.getHeader().originalSenderNameToString();
      outputSocket->send(packetToSend.toBinary());
      hasToken = false;
      //      std::cout << "[INFO] Sending to: "
      //                << outputSocket->getConnectionIp().to_string() << ':'
      //                << outputSocket->getConnectionPort() << std::endl;
      //      outputSocket->sendTo(packetToSend.toBinary(), neighborIp,
      //      neighborPort);
    }
  }
}
