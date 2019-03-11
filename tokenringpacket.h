#ifndef TOKENRINGPACKET_H
#define TOKENRINGPACKET_H

#include <cstdint>

#include <stdexcept>
#include <vector>

#include "ip4.h"

using TokenRingPacketException = std::runtime_error;

using TokenRingPacketInputBufferTooSmallException = TokenRingPacketException;

class TokenRingPacket {
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

#pragma pack(push, 1)
  class Header {
   public:
    PacketType type;

    TokenStatus_t tokenStatus;

    char originalSenderName[UserIdentifierNameSize];  // padded with zeros,
                                                      // maximum chars = 15

    char packetSenderName[UserIdentifierNameSize];  // padded with zeros,
                                                    // maximum chars = 15

    char packetReceiverName[UserIdentifierNameSize];

    uint16_t dataSize;  // size of data

    void setOriginalSenderName(const std::string& str);

    void setPacketSenderName(const std::string& str);

    void setPacketReceiverName(const std::string& str);

    std::string originalSenderNameToString() const;

    std::string packetSenderNameToString() const;

    std::string packetReceiverNameToString() const;
  };
#pragma pack(pop)

#pragma pack(push, 1)
  class RegisterSubheader {
   public:
    // New host data
    Ip4 ip;
    unsigned short port;

    // New host neighbor name
    char neighborToDisconnectName[UserIdentifierNameSize];

    void setNeighborToDisconnectName(const std::string& str);

    std::string neighborToDisconnectNameToString() const;
  };
#pragma pack(pop)

 private:
  Header header;
  std::vector<unsigned char> data;

  void constructHeaderFromBinaryData(
      const std::vector<unsigned char>& sourceBuffer);

  void extractDataFromBinaryAndHeader(
      const std::vector<unsigned char>& sourceBuffer);

 public:
  TokenRingPacket();

  explicit TokenRingPacket(
      const std::vector<unsigned char>& sourceBuffer) noexcept(false);

  const Header& getHeader() const;

  void setHeader(const Header& value);

  const std::vector<unsigned char>& getData() const;

  void setData(const std::vector<unsigned char>& value);

  std::vector<unsigned char> toBinary() const;

  void fromBinary(const std::vector<unsigned char> &sourceBuffer) noexcept(false);
};

#endif  // TOKENRINGPACKET_H
