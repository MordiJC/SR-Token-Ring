#ifndef TOKENRINGPACKET_H
#define TOKENRINGPACKET_H

#include <cstdint>

#include <array>
#include <stdexcept>
#include <vector>

#include "ip4.h"
#include "serializable.h"

using TokenRingPacketException = std::runtime_error;

using TokenRingPacketInputBufferTooSmallException = TokenRingPacketException;

using TokenRingPacketTooMuchDataException = TokenRingPacketException;

class TokenRingPacket : public Serializable {
 public:
  enum class PacketType : uint8_t {
    NONE = 0u,  /// none type representing dummy packet
    REGISTER,   /// Client registration packet. (Register me with this name)
    JOIN,  /// Join is type that is used only when telling host that we want to
           /// join ring. Other ring hosts will be informed using REGISTER type
           /// packet
    DATA,

    PACKET_TYPE_NUM  /// Number of packet types. DO NOT USE AS TYPE!!!
  };

  using TokenStatus_t = uint8_t;

  static const size_t UserIdentifierNameSize = 16;

  static const size_t DataMaxSize = 512;

#pragma pack(push, 1)
  struct Header {
    PacketType type;
    TokenStatus_t tokenStatus;

    char originalSenderName[UserIdentifierNameSize];  // padded with zeros,
                                                      // maximum chars = 15
    char packetSenderName[UserIdentifierNameSize];    // padded with zeros,
                                                      // maximum chars = 15
    char packetReceiverName[UserIdentifierNameSize];

    // used in REGISTER packet

    char neighborToDisconnectName[UserIdentifierNameSize];

    Ip4 registerIp;

    unsigned short registerPort;

    uint16_t dataSize;
  };
#pragma pack(pop)

 private:
  Header header;
  std::array< char, DataMaxSize> data;

  Serializable::size_type constructHeaderFromBinaryData(
      const std::vector<unsigned char>& sourceBuffer);

  Serializable::size_type extractDataFromBinaryAndHeader(
      const std::vector<unsigned char>& sourceBuffer);

 public:
  TokenRingPacket();

  explicit TokenRingPacket(
      const std::vector<unsigned char>& sourceBuffer) noexcept(false);

  virtual ~TokenRingPacket() = default;

  const Header& getHeader() const;

  void setHeader(const Header& value);

  const std::vector<unsigned char> getData() const;

  const std::vector<char> getDataAsSignedCharsVector() const;

  void setData(const std::vector<unsigned char>& value);

  Serializable::size_type fromBinary(
      const Serializable::container_type& buffer) noexcept(false);
  Serializable::container_type toBinary() const;
};

#endif  // TOKENRINGPACKET_H
