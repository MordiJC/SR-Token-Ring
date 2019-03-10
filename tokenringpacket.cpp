#include "tokenringpacket.h"

#include <algorithm>
#include <cstring>

TokenRingPacket::TokenRingPacket() {}

void TokenRingPacket::constructHeaderFromBinaryData(
    const std::vector<unsigned char> &sourceBuffer) {
  if (sourceBuffer.size() < sizeof(Header)) {
    throw TokenRingPacketInputBufferTooSmallException(
        "Passed input buffer is too small");
  }

  std::memcpy(&header, sourceBuffer.data(), sizeof(header));
}

void TokenRingPacket::extractDataFromBinaryAndHeader(
    const std::vector<unsigned char> &sourceBuffer) {
  if (sourceBuffer.size() - sizeof(header) < header.dataSize) {
    throw TokenRingPacketInputBufferTooSmallException(
        "Passed input buffer contains less data than declared in header");
  }

  data.reserve(header.dataSize);

  std::memcpy(data.data(), &sourceBuffer.data()[sizeof(header)],
              header.dataSize);
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

std::vector<unsigned char> TokenRingPacket::toBinary() const {
  std::vector<unsigned char> buffer;
  buffer.reserve(sizeof(header) + data.size());

  std::memcpy(buffer.data(), &header, sizeof(header));

  std::copy(data.begin(), data.end(), buffer.data() + sizeof(header));

  return buffer;
}

void TokenRingPacket::fromBinary(const std::vector<unsigned char>& sourceBuffer) noexcept(false)
{
  constructHeaderFromBinaryData(sourceBuffer);

  extractDataFromBinaryAndHeader(sourceBuffer);
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

void TokenRingPacket::RegisterSubheader::setNeighborToDisconnectName(const std::string &str)
{
  std::memset(neighborToDisconnectName, 0, UserIdentifierNameSize);
  std::memcpy(neighborToDisconnectName, str.c_str(),
              std::min(UserIdentifierNameSize - 1, str.size()));
}
