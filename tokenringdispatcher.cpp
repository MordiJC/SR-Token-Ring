#include "tokenringdispatcher.h"
#include "quitstatusobserver.h"

#include <algorithm>
#include <cstring>
#include <iostream>
#include <thread>

#include "utility.h"

const std::string TokenRingDispatcher::workingIp{"127.0.0.1"};

void TokenRingDispatcher::initializeSockets() {
  Ip4 workingIp4 = Ip4_from_string(workingIp);
  std::cout << "[INFO] Input socket binding to " << to_string(workingIp4) << ':'
            << programArguments.getPort() << std::endl;
  inputSocket->bind(workingIp4, programArguments.getPort());
  std::cout << "[INFO] Input socket bound to " << to_string(workingIp4) << ':'
            << programArguments.getPort() << std::endl;

  inputSocket->listen(2);
  std::cout << "[INFO] Input socket listening with backlog of 2" << std::endl;

  std::lock_guard<std::recursive_mutex> guard(outputSocketMutex);

  outputSocket->connect(programArguments.getNeighborIp(),
                        programArguments.getNeighborPort());

  std::cout << "[INFO] Output socket connected "
            << to_string(outputSocket->getIp()) << ':'
            << outputSocket->getPort() << " -> "
            << to_string(programArguments.getNeighborIp()) << ':'
            << programArguments.getNeighborPort() << std::endl;
}

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

void TokenRingDispatcher::sendJoinRequest() {
  TokenRingPacket joinPacket = createJoinPacket();

  std::lock_guard<std::recursive_mutex> guard(outputSocketMutex);

  std::cout << "[INFO] Sending JOIN request to "
            << to_string(outputSocket->getConnectionIp()) << ':'
            << outputSocket->getConnectionPort() << std::endl;

  outputSocket->send(joinPacket.toBinary());
}

void TokenRingDispatcher::preparePacketRegisterSubheaderFromJoinPacket(
    Socket& incomingSocket, TokenRingPacket::Header& header) {
  insertStringToCharArrayWithLength(programArguments.getUserIdentifier(),
                                    header.neighborToDisconnectName,
                                    TokenRingPacket::UserIdentifierNameSize);
  header.registerIp = incomingSocket.getConnectionIp();
  header.registerPort = incomingSocket.getConnectionPort();
}

void TokenRingDispatcher::handleDataPacket(TokenRingPacket& incomingPacket) {
  if (incomingPacket.getHeader().tokenStatus) {
    tokenRingHosts.insert(incomingPacket.getHeader().originalSenderName);
    tokenRingHosts.insert(incomingPacket.getHeader().packetSenderName);
    if (incomingPacket.getHeader().packetReceiverName ==
        programArguments.getUserIdentifier()) {
      std::string::size_type strSize = incomingPacket.getHeader().dataSize;

      std::string message{incomingPacket.getDataAsCharsVector().data(),
                          strSize};

      std::cout << "[Host: @" << programArguments.getUserIdentifier()
                << "][WORKER] Received data: `" << message << "\'" << std::endl;
    } else {
      TokenRingPacket::Header header = incomingPacket.getHeader();
      insertStringToCharArrayWithLength(
          programArguments.getUserIdentifier(), header.packetSenderName,
          TokenRingPacket::UserIdentifierNameSize);
      insertStringToCharArrayWithLength(
          programArguments.getUserIdentifier(), header.packetSenderName,
          TokenRingPacket::UserIdentifierNameSize);

      incomingPacket.setHeader(header);
      std::lock_guard<std::recursive_mutex> guard(dataPacketsMutex);
      dataPackets.push(incomingPacket);
    }

    hasToken = true;
  }
  std::cout << "[INFO][WORKER] Received DATA packet withoud token."
            << std::endl;
}

void TokenRingDispatcher::handleJoinPacket(TokenRingPacket& incomingPacket,
                                           Socket& incomingSocket) {
  using trppt = TokenRingPacket::PacketType;

  const TokenRingPacket::Header& packetHeaderRef = incomingPacket.getHeader();

  std::cout << "INCOMING JOIN " << incomingPacket.to_string() << std::endl;

  if (incomingPacket.getHeader().originalSenderName ==
      programArguments.getUserIdentifier()) {
    std::cout << "[INFO] Adding self to token ring hosts list" << std::endl;

    tokenRingHosts.insert(incomingPacket.getHeader().originalSenderName);
    previousHostName = incomingPacket.getHeader().originalSenderName;

    neighborIp = packetHeaderRef.registerIp;
    neighborPort = packetHeaderRef.registerPort;

  } else {
    tokenRingHosts.insert(incomingPacket.getHeader().originalSenderName);

    std::lock_guard<std::recursive_mutex> guard(outputSocketMutex);
    outputSocket->close();
    delete outputSocket.release();
    outputSocket = std::make_unique<Socket>(programArguments.getProtocol());
    outputSocket->connect(incomingPacket.getHeader().registerIp,
                          incomingPacket.getHeader().registerPort);

    neighborIp = incomingPacket.getHeader().registerIp;
    neighborPort = incomingPacket.getHeader().registerPort;

    std::cout << "[INFO][WORKER][CONNECTING] "
              << to_string(outputSocket->getIp()) << ':'
              << outputSocket->getPort() << " to "
              << to_string(outputSocket->getConnectionIp()) << ':'
              << outputSocket->getConnectionPort() << std::endl;

    if (previousHostName != programArguments.getUserIdentifier()) {
      std::cout << "[INFO] Preparing REGISTER header" << std::endl;
      TokenRingPacket::Header header = incomingPacket.getHeader();
      header.type = trppt::REGISTER;
      preparePacketRegisterSubheaderFromJoinPacket(incomingSocket, header);

      incomingPacket.setHeader(header);

      std::cout << "NEW REGISTER " << incomingPacket.to_string() << std::endl;

      std::lock_guard<std::recursive_mutex> guard(registerRequestPacketsMutex);
      registerRequestPackets.push(incomingPacket);
    }
    previousHostName = incomingPacket.getHeader().originalSenderName;
  }

  if (tokenRingInputProcessingThread) {
    tokenRingInputProcessingThreadDone = true;
    if (tokenRingInputProcessingThread->joinable()) {
      tokenRingInputProcessingThread->join();
    }
  }

  tokenRingInputProcessingThreadDone = false;
  tokenRingInputProcessingThreadSocketHolder =
      std::make_unique<Socket>(incomingSocket);
  tokenRingInputProcessingThread.reset(
      new std::thread(&TokenRingDispatcher::processNeighborSocket, this,
                      std::ref(*tokenRingInputProcessingThreadSocketHolder),
                      std::ref(tokenRingInputProcessingThreadDone)));
}

void TokenRingDispatcher::handleRegisterPacket(
    TokenRingPacket& incomingPacket) {
  std::cout << "[INFO][WORKER] Handling incoming REGISTER packet" << std::endl;

  std::cout << "INCOMING REGISTER " << incomingPacket.to_string() << std::endl;

  tokenRingHosts.insert(incomingPacket.getHeader().originalSenderName);
  tokenRingHosts.insert(incomingPacket.getHeader().packetSenderName);

  if (incomingPacket.getHeader().neighborToDisconnectName ==
      programArguments.getUserIdentifier()) {
    std::lock_guard<std::recursive_mutex> guard(outputSocketMutex);

    outputSocket->close();
    delete outputSocket.release();
    outputSocket = std::make_unique<Socket>(programArguments.getProtocol());
    outputSocket->connect(incomingPacket.getHeader().registerIp,
                          incomingPacket.getHeader().registerPort);

    neighborIp = incomingPacket.getHeader().registerIp;
    neighborPort = incomingPacket.getHeader().registerPort;

    std::cout << "[INFO][WORKER][CONNECTING] "
              << to_string(outputSocket->getIp()) << ':'
              << outputSocket->getPort() << " to "
              << to_string(outputSocket->getConnectionIp()) << ':'
              << outputSocket->getConnectionPort() << std::endl;
  } else {
    std::lock_guard<std::recursive_mutex> guard(registerRequestPacketsMutex);
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

void TokenRingDispatcher::processNeighborSocket(Socket neighborSocket,
                                                std::atomic_bool& done) {
  std::cout << "[THREAD: " << std::this_thread::get_id() << "] Starting."
            << std::endl;
  TokenRingPacket incomingPacket;
  Serializable::container_type incomingBuffer;

  while (!QuitStatusObserver::getInstance().shouldQuit() && done == false) {
    try {
      if (programArguments.getProtocol() == Protocol::TCP) {
        std::cout << "[INFO][WORKER] Receiving from: "
                  << to_string(neighborSocket.getConnectionIp()) << ':'
                  << neighborSocket.getConnectionPort() << std::endl;

        incomingBuffer = neighborSocket.receive();

        std::cout << "[INFO][WORKER] Received packet from "
                  << to_string(neighborSocket.getConnectionIp()) << ':'
                  << neighborSocket.getConnectionPort()
                  << " with size of size: " << incomingBuffer.size()
                  << std::endl;
      } else {
        incomingBuffer = neighborSocket.receiveFrom().second;
      }

    } catch (const SocketReceivingFailedException& ex) {
      std::cerr << "[ERROR][WORKER] Socket receiving failed: " << ex.what()
                << std::endl;
      continue;
    }

    try {
      size_t extractedBytes = incomingPacket.fromBinary(incomingBuffer);
      std::cout << "[INFO][WORKER] Extracted bytes: " << extractedBytes
                << std::endl;

    } catch (const TokenRingPacketException& ex) {
      std::cerr << "[ERROR][WORKER] Token Ring Packet creation failed: "
                << ex.what() << std::endl;
      continue;
    }

    using trppt = TokenRingPacket::PacketType;

    bool shouldPass = false;

    switch (incomingPacket.getHeader().type) {
      case trppt::DATA:
        std::cout << "[INFO][WORKER] Handling incoming DATA packet"
                  << std::endl;
        handleDataPacket(incomingPacket);
        break;
      case trppt::REGISTER: {
        try {
          handleRegisterPacket(incomingPacket);
        } catch (
            const TokenRingDispatcherNotEnoughDataForRegisterSubheaderException&
                ex) {
          std::cerr << "[ERROR][WORKER] " << ex.what() << std::endl;
        }
      } break;
      default:
        std::cerr << "[ERROR][WORKER] Unknown packet type. Packet type code: "
                  << static_cast<std::underlying_type_t<trppt>>(
                         incomingPacket.getHeader().type)
                  << std::endl;
        shouldPass = true;
    }

    if (shouldPass) {
      continue;
    }

    if (!hasToken) {
      std::cout << "[INFO][WORKER] No token, passing." << std::endl;
      continue;
    }

    // Handle packets from queues and send them

    if (!registerRequestPackets.empty()) {
      TokenRingPacket& packetToSend = registerRequestPackets.front();
      std::lock_guard<std::recursive_mutex> guard(registerRequestPacketsMutex);
      registerRequestPackets.pop();

      std::lock_guard<std::recursive_mutex> outputGuard(outputSocketMutex);

      outputSocket->send(packetToSend.toBinary());
      hasToken = false;
    } else if (!dataPackets.empty()) {
      TokenRingPacket& packetToSend = dataPackets.front();

      if (packetToSend.getHeader().originalSenderName == lastDataPacketSender) {
        std::cout << "[INFO][WORKER] Sending greetings packet (1-1)"
                  << std::endl;
        TokenRingPacket greetingsPacket =
            createAndPrepareGreetingsPacketToSend();
        lastDataPacketSender = greetingsPacket.getHeader().originalSenderName;

        {
          std::lock_guard<std::recursive_mutex> outputGuard(outputSocketMutex);

          outputSocket->send(greetingsPacket.toBinary());
        }
        std::this_thread::sleep_for(std::chrono::seconds{2});
      } else {
        std::lock_guard<std::recursive_mutex> guard(dataPacketsMutex);
        std::cout << "[INFO][WORKER] Sending data packet to next node"
                  << std::endl;
        dataPackets.pop();
        lastDataPacketSender = packetToSend.getHeader().originalSenderName;

        std::lock_guard<std::recursive_mutex> outputGuard(outputSocketMutex);

        outputSocket->send(packetToSend.toBinary());
      }
      hasToken = false;
    } else {
      {
        std::lock_guard<std::recursive_mutex> outputGuard(outputSocketMutex);
        std::cout << "[INFO][WORKER] Sending greetings packet (2-1)"
                  << std::endl;
        std::cout << "[INFO][WORKER] Sending from "
                  << to_string(outputSocket->getIp()) << ':'
                  << outputSocket->getPort() << " to "
                  << to_string(outputSocket->getConnectionIp()) << ':'
                  << outputSocket->getConnectionPort() << std::endl;

        TokenRingPacket packetToSend = createAndPrepareGreetingsPacketToSend();
        lastDataPacketSender = packetToSend.getHeader().originalSenderName;
        outputSocket->send(packetToSend.toBinary());
      }
      std::this_thread::sleep_for(std::chrono::seconds{2});
      hasToken = false;
    }
  }

  std::cout << "[THREAD: " << std::this_thread::get_id() << "] Terminating."
            << std::endl;
}

TokenRingDispatcher::TokenRingDispatcher(ProgramArguments args)
    : previousHostName(args.getUserIdentifier()),
      programArguments(args),
      hasToken(args.getHasToken()) {
  outputSocket = std::make_unique<Socket>(args.getProtocol());
  inputSocket = std::make_unique<Socket>(args.getProtocol());
}

void TokenRingDispatcher::run() {
  initializeSockets();

  sendJoinRequest();

  TokenRingPacket incomingPacket;
  Serializable::container_type incomingBuffer;

  while (!QuitStatusObserver::getInstance().shouldQuit()) {
    try {
      std::cout << "[INFO] Waiting for connection" << std::endl;

      Socket incomingSocket{inputSocket->select()};

      std::cout << "[INFO] Select passed positive" << std::endl;

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

      if (incomingPacket.getHeader().type == trppt::JOIN) {
        std::cout << "[INFO] Handling incoming JOIN packet" << std::endl;
        handleJoinPacket(incomingPacket, incomingSocket);
      } else {
        std::cout << "[WARNIING] Client trying to send packets other than JOIN "
                     "before joining to ring."
                  << std::endl;
        std::cout << incomingPacket.to_string() << std::endl;

        incomingSocket.close();
        continue;
      }
    } catch (const SocketSelectTimeoutException& ex) {
      std::cout << "[INFO] " << ex.what() << std::endl;
    }

    if (!registerRequestPackets.empty()) {
      TokenRingPacket& packetToSend = registerRequestPackets.front();
      std::lock_guard<std::recursive_mutex> guard(registerRequestPacketsMutex);
      registerRequestPackets.pop();

      std::lock_guard<std::recursive_mutex> outputGuard(outputSocketMutex);

      outputSocket->send(packetToSend.toBinary());
      hasToken = false;
    } else {
      std::cout << "[INFO][WORKER] Sending greetings packet (1-1)" << std::endl;
      TokenRingPacket greetingsPacket = createAndPrepareGreetingsPacketToSend();
      lastDataPacketSender = greetingsPacket.getHeader().originalSenderName;
      outputSocket->send(greetingsPacket.toBinary());
      std::this_thread::sleep_for(std::chrono::seconds{2});
    }
  }

  if (tokenRingInputProcessingThread &&
      tokenRingInputProcessingThread->joinable()) {
    tokenRingInputProcessingThread->join();
  }
}
