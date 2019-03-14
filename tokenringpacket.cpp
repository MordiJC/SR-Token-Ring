#include "tokenringpacket.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <sstream>

TokenRingPacket::TokenRingPacket() {}

Serializable::size_type TokenRingPacket::constructHeaderFromBinaryData(
    const std::vector<unsigned char> &sourceBuffer) {
  if (sourceBuffer.size() < sizeof(header)) {
    throw TokenRingPacketInputBufferTooSmallException(
        "Passed input buffer is too small");
  }

  std::memcpy(&header, sourceBuffer.data(), sizeof(header));

  return sizeof(header);
}

Serializable::size_type TokenRingPacket::extractDataFromBinaryAndHeader(
    const std::vector<unsigned char> &sourceBuffer) {
  if (sourceBuffer.size() - sizeof(header) < header.dataSize) {
    throw TokenRingPacketInputBufferTooSmallException(
        "Passed input buffer contains less data than declared in header");
  }

  std::memcpy(data.data(), sourceBuffer.data() + sizeof(header), DataMaxSize);

  return DataMaxSize;
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

const std::vector<unsigned char> TokenRingPacket::getData() const {
  return {data.begin(), data.begin() + header.dataSize};
}

const std::vector<char> TokenRingPacket::getDataAsCharsVector() const {
  return {data.begin(), data.begin() + header.dataSize};
}

void TokenRingPacket::setData(const std::vector<unsigned char> &value) {
  if (value.size() > DataMaxSize) {
    throw TokenRingPacketTooMuchDataException(
        "Passed data is too big. You have to perform fragmentation using "
        "DataMaxSize as max value.");
  }
  std::memcpy(data.data(), value.data(), value.size());
  header.dataSize = static_cast<uint16_t>(value.size());
}

std::string TokenRingPacket::to_string() const {
  std::stringstream out;
  out << "TokenRingPacket::Header:" << std::endl
      << "Type: JOIN" << std::endl
      << "TokenStatus: Available" << std::endl
      << "PacketSender: " << header.packetSenderName << std::endl
      << "OriginalSender: " << header.originalSenderName << std::endl
      << "PacketReceiver: " << header.packetReceiverName << std::endl
      << "DataSize: " << header.dataSize << std::endl
      << "RegisterIP: " << ::to_string(header.registerIp) << std::endl
      << "RegisterPort: " << header.registerPort << std::endl
      << "NeighborToDisconnect: " << header.neighborToDisconnectName;

  return out.str();
}

Serializable::container_type TokenRingPacket::toBinary() const {
  std::vector<unsigned char> buffer;

  buffer.resize(sizeof(header));
  std::memcpy(buffer.data(), &header, sizeof(header));

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
