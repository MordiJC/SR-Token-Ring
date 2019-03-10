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
        dataPackets.push(incomingPacket);
        break;

      case trppt::JOIN: {
        TokenRingPacket::Header header = incomingPacket.getHeader();
        header.type = trppt::REGISTER;
        incomingPacket.setHeader(header);
        registerRequestPackets.push(incomingPacket);

        // TODO: perform registration routines
      } break;
      case trppt::REGISTER:
        registerRequestPackets.push(incomingPacket);
        break;
      default:
        std::cerr << "[ERROR] Unknown packet type. Packet type code: "
                  << static_cast<std::underlying_type_t<trppt>>(
                         incomingPacket.getHeader().type)
                  << std::endl;
    }

    // Handle packets from queues and send them
  }
}
