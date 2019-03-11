#include "tokenringdispatcher.h"
#include "quitstatusobserver.h"

#include <algorithm>
#include <cstring>
#include <iostream>

TokenRingPacket TokenRingDispatcher::createJoinPacket() {
  TokenRingPacket packet;

  TokenRingPacket::Header packetHeader;
  packetHeader.type = TokenRingPacket::PacketType::JOIN;
  packetHeader.tokenStatus = 1;
  packetHeader.setPacketSenderName(programArguments.getUserIdentifier());
  packetHeader.setOriginalSenderName(programArguments.getUserIdentifier());

  TokenRingPacket::RegisterSubheader registerSubheader;
  registerSubheader.ip = Ip4("127.0.0.1");
  registerSubheader.port = programArguments.getPort();
  registerSubheader.setNeighborToDisconnectName("");

  std::vector<unsigned char> data;
  data.reserve(sizeof(registerSubheader));

  packet.setHeader(packetHeader);
  packet.setData(data);

  return packet;
}

TokenRingDispatcher::TokenRingDispatcher(ProgramArguments args)
    : programArguments(args), hasToken(args.getHasToken()) {
  outputSocket = std::make_unique<Socket>(args.getProtocol());
  neighborSocket = std::make_unique<Socket>(args.getProtocol());
}

void TokenRingDispatcher::sendJoinRequest() {
  TokenRingPacket joinPacket = createJoinPacket();

  outputSocket->send(joinPacket.toBinary());
}

void TokenRingDispatcher::initializeSockets() {
  neighborSocket->bind(Ip4("127.0.0.1"), programArguments.getPort());

  neighborSocket->listen(2);

  outputSocket->connect(programArguments.getNeighborIp(),
                        programArguments.getNeighborPort());
}

std::vector<unsigned char> TokenRingDispatcher::prepareRegisterPacket(
    Socket& incomingSocket) {
  TokenRingPacket::RegisterSubheader registerSubheader;
  registerSubheader.setNeighborToDisconnectName(
      programArguments.getUserIdentifier());
  registerSubheader.ip = incomingSocket.getConnectionIp();
  registerSubheader.port = incomingSocket.getConnectionPort();

  std::vector<unsigned char> registerSubheaderBinaryForm;
  registerSubheaderBinaryForm.resize(sizeof(registerSubheader));
  std::memcpy(registerSubheaderBinaryForm.data(), &registerSubheader,
              sizeof(registerSubheader));

  return registerSubheaderBinaryForm;
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
  TokenRingPacket::Header header = incomingPacket.getHeader();
  header.type = trppt::REGISTER;
  incomingPacket.setHeader(header);

  // Action performed when no neighbor is set
  // (creating ring from one host)
  if (neighborName.empty() && !neighborSocket) {  // First host in ring
    neighborName = incomingPacket.getHeader().originalSenderNameToString();

    neighborSocket = std::make_unique<Socket>(incomingSocket);
  } else if (!neighborName.empty() && neighborSocket) {
    std::vector<unsigned char> registerSubheaderBinaryForm =
        prepareRegisterPacket(incomingSocket);

    incomingPacket.setData(registerSubheaderBinaryForm);

    registerRequestPackets.push(incomingPacket);
  } else {
    std::cerr << "[ERROR] Invalid state! Terminating!" << std::endl;
    QuitStatusObserver::getInstance().quit();
  }
}

void TokenRingDispatcher::run() {
  initializeSockets();

  sendJoinRequest();

  TokenRingPacket incomingPacket;
  std::vector<unsigned char> incomingBuffer;

  while (!QuitStatusObserver::getInstance().shouldQuit()) {
    Socket incomingSocket{neighborSocket->select()};

    try {
      incomingBuffer = incomingSocket.receive();

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

    switch (incomingPacket.getHeader().type) {
      case trppt::DATA:
        handleIncomingDataPacket(incomingPacket);
        break;

      case trppt::JOIN:
        handleIncomingJoinPacket(incomingPacket, incomingSocket);
        break;
      case trppt::REGISTER: {
        TokenRingPacket::RegisterSubheader registerSubheader;

        if (incomingPacket.getHeader().dataSize < sizeof(registerSubheader)) {
          throw TokenRingDispatcherNotEnoughDataForRegisterSubheaderException(
              "Incoming packet has not enough data to construct Register "
              "Subheader");
        }

        std::memcpy(&registerSubheader, incomingPacket.getData().data(),
                    sizeof(registerSubheader));

        if (registerSubheader.neighborToDisconnectNameToString() ==
            neighborName) {
          // TODO: Disconnect neighbor and connect new
        } else {
          registerRequestPackets.push(incomingPacket);
        }
      } break;
      default:
        std::cerr << "[ERROR] Unknown packet type. Packet type code: "
                  << static_cast<std::underlying_type_t<trppt>>(
                         incomingPacket.getHeader().type)
                  << std::endl;
    }

    // Handle packets from queues and send them
    if (!registerRequestPackets.empty()) {
      TokenRingPacket packetToSend = registerRequestPackets.front();
      registerRequestPackets.pop();

      outputSocket->send(packetToSend.toBinary());
    } else if (!dataPackets.empty()) {
      TokenRingPacket packetToSend = dataPackets.front();
      dataPackets.pop();

      outputSocket->send(packetToSend.toBinary());
    }
    // else: nothing to do. Check if has token and send data.
  }
}
