#include "tokenringpacket.h"

#include <algorithm>
#include <array>
#include <cstring>

TokenRingPacket::TokenRingPacket() {}

Serializable::size_type TokenRingPacket::constructHeaderFromBinaryData(
    const std::vector<unsigned char> &sourceBuffer) {
  if (sourceBuffer.size() < sizeof(Header)) {
    throw TokenRingPacketInputBufferTooSmallException(
        "Passed input buffer is too small");
  }

  return header.fromBinary(sourceBuffer);
}

Serializable::size_type TokenRingPacket::extractDataFromBinaryAndHeader(
    const std::vector<unsigned char> &sourceBuffer) {
  if (sourceBuffer.size() - sizeof(header) < header.dataSize) {
    throw TokenRingPacketInputBufferTooSmallException(
        "Passed input buffer contains less data than declared in header");
  }

  data.resize(header.dataSize);

  std::memcpy(data.data(), &sourceBuffer.data()[sizeof(header)],
              header.dataSize);

  return header.dataSize;
}

TokenRingPacket::TokenRingPacket(
    const std::vector<unsigned char> &sourceBuffer) noexcept(false) {
  fromBinary(sourceBuffer);
}

const TokenRingPacket::Header &TokenRingPacket::getHeader() const {
  return header;
}

void TokenRingPacket::setHeader(const Header &value) {
  header = value;
  header.dataSize = static_cast<uint16_t>(data.size());
}

const std::vector<unsigned char> &TokenRingPacket::getData() const {
  return data;
}

void TokenRingPacket::setData(const std::vector<unsigned char> &value) {
  data = value;
  header.dataSize = static_cast<uint16_t>(data.size());
}

Serializable::container_type TokenRingPacket::toBinary() const {
  std::vector<unsigned char> buffer;

  buffer = header.toBinary();

  buffer.insert(buffer.end(), data.begin(), data.end());

  return buffer;
}

Serializable::size_type TokenRingPacket::fromBinary(
    const Serializable::container_type &sourceBuffer) noexcept(false) {
  Serializable::size_type res = 0;
  res += constructHeaderFromBinaryData(sourceBuffer);

  res += extractDataFromBinaryAndHeader(sourceBuffer);

  return res;
}

void TokenRingPacket::Header::setOriginalSenderName(const std::string &str) {
  std::memset(originalSenderName, 0, UserIdentifierNameSize);
  std::memcpy(originalSenderName, str.c_str(),
              std::min(UserIdentifierNameSize - 1, str.size()));
}

void TokenRingPacket::Header::setPacketSenderName(const std::string &str) {
  std::memset(packetSenderName, 0, UserIdentifierNameSize);
  std::memcpy(packetSenderName, str.c_str(),
              std::min(UserIdentifierNameSize - 1, str.size()));
}

void TokenRingPacket::Header::setPacketReceiverName(const std::string &str) {
  std::memset(packetReceiverName, 0, UserIdentifierNameSize);
  std::memcpy(packetReceiverName, str.c_str(),
              std::min(UserIdentifierNameSize - 1, str.size()));
}

std::string TokenRingPacket::Header::originalSenderNameToString() const {
  if (originalSenderName[TokenRingPacket::UserIdentifierNameSize - 1] == 0) {
    return std::string(originalSenderName);
  } else {
    return std::string(
        originalSenderName,
        originalSenderName + TokenRingPacket::UserIdentifierNameSize);
  }
}

std::string TokenRingPacket::Header::packetSenderNameToString() const {
  if (packetSenderName[TokenRingPacket::UserIdentifierNameSize - 1] == 0) {
    return std::string(packetSenderName);
  } else {
    return std::string(
        packetSenderName,
        packetSenderName + TokenRingPacket::UserIdentifierNameSize);
  }
}

std::string TokenRingPacket::Header::packetReceiverNameToString() const {
  if (packetReceiverName[TokenRingPacket::UserIdentifierNameSize - 1] == 0) {
    return std::string(packetReceiverName);
  } else {
    return std::string(
        packetReceiverName,
        packetReceiverName + TokenRingPacket::UserIdentifierNameSize);
  }
}

Serializable::size_type TokenRingPacket::Header::fromBinary(
    const Serializable::container_type &buffer) {
  size_t offset = 0;

  std::memcpy(&type, buffer.data() + offset, sizeof(PacketType));
  offset += sizeof(PacketType);

  std::memcpy(&tokenStatus, buffer.data() + offset, sizeof(TokenStatus_t));
  offset += sizeof(TokenStatus_t);

  std::memcpy(&originalSenderName[0], buffer.data() + offset,
              sizeof(originalSenderName));
  offset += sizeof(originalSenderName);

  std::memcpy(&packetSenderName[0], buffer.data() + offset,
              sizeof(packetSenderName));
  offset += sizeof(packetSenderName);

  std::memcpy(&packetReceiverName[0], buffer.data() + offset,
              sizeof(packetReceiverName));
  offset += sizeof(packetReceiverName);

  std::memcpy(&dataSize, buffer.data() + offset, sizeof(dataSize));
  offset += sizeof(dataSize);

  return offset;
}

Serializable::container_type TokenRingPacket::Header::toBinary() const {
  Serializable::container_type buffer;

  std::array<Serializable::data_type, sizeof(type)> typeBuffer;
  std::memcpy(typeBuffer.data(), &type, sizeof(type));
  buffer.insert(buffer.end(), typeBuffer.begin(), typeBuffer.end());

  std::array<Serializable::data_type, sizeof(tokenStatus)> tokenStatusBuffer;
  std::memcpy(tokenStatusBuffer.data(), &tokenStatus, sizeof(tokenStatus));
  buffer.insert(buffer.end(), tokenStatusBuffer.begin(),
                tokenStatusBuffer.end());

  std::array<Serializable::data_type, sizeof(originalSenderName)>
      originalSenderNameBuffer;
  std::memcpy(originalSenderNameBuffer.data(), originalSenderName,
              sizeof(originalSenderName));
  buffer.insert(buffer.end(), originalSenderNameBuffer.begin(),
                originalSenderNameBuffer.end());

  std::array<Serializable::data_type, sizeof(originalSenderName)>
      packetSenderNameBuffer;
  std::memcpy(packetSenderNameBuffer.data(), packetSenderName,
              sizeof(packetSenderName));
  buffer.insert(buffer.end(), packetSenderNameBuffer.begin(),
                packetSenderNameBuffer.end());

  std::array<Serializable::data_type, sizeof(originalSenderName)>
      packetReceiverNameBuffer;
  std::memcpy(packetReceiverNameBuffer.data(), packetReceiverName,
              sizeof(packetReceiverName));
  buffer.insert(buffer.end(), packetReceiverNameBuffer.begin(),
                packetReceiverNameBuffer.end());

  return buffer;
}

void TokenRingPacket::RegisterSubheader::setNeighborToDisconnectName(
    const std::string &str) {
  std::memset(neighborToDisconnectName, 0, UserIdentifierNameSize);
  std::memcpy(neighborToDisconnectName, str.c_str(),
              std::min(UserIdentifierNameSize - 1, str.size()));
}

std::string
TokenRingPacket::RegisterSubheader::neighborToDisconnectNameToString() const {
  if (neighborToDisconnectName[TokenRingPacket::UserIdentifierNameSize - 1] ==
      0) {
    return std::string(neighborToDisconnectName);
  } else {
    return std::string(
        neighborToDisconnectName,
        neighborToDisconnectName + TokenRingPacket::UserIdentifierNameSize);
  }
}

Serializable::size_type TokenRingPacket::RegisterSubheader::fromBinary(
    const Serializable::container_type &data) {
  size_t offset = 0;

  offset = ip.fromBinary(data);

  std::memcpy(&port, data.data() + offset, sizeof(PacketType));
  offset += sizeof(PacketType);

  return offset;
}

Serializable::container_type TokenRingPacket::RegisterSubheader::toBinary()
    const {
  Serializable::container_type buffer;

  buffer = ip.toBinary();

  std::array<Serializable::data_type, sizeof(port)> portBuffer;
  std::memcpy(portBuffer.data(), &port, sizeof(port));
  buffer.insert(buffer.end(), portBuffer.begin(), portBuffer.end());

  return buffer;
}
