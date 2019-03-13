#ifndef TOKENRINGPACKET_H
#define TOKENRINGPACKET_H

#include <cstdint>

#include <stdexcept>
#include <vector>

#include "ip4.h"
#include "serializable.h"

using TokenRingPacketException = std::runtime_error;

using TokenRingPacketInputBufferTooSmallException = TokenRingPacketException;

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

#pragma pack(push, 1)
  class Header : public Serializable {
   public:
    PacketType type;
    TokenStatus_t tokenStatus;

    char originalSenderName[UserIdentifierNameSize];  // padded with zeros,
                                                      // maximum chars = 15
    char packetSenderName[UserIdentifierNameSize];    // padded with zeros,
                                                      // maximum chars = 15
    char packetReceiverName[UserIdentifierNameSize];

    uint16_t dataSize;  // size of data

   public:
    virtual ~Header() = default;

    void setOriginalSenderName(const std::string& str);

    void setPacketSenderName(const std::string& str);

    void setPacketReceiverName(const std::string& str);

    std::string originalSenderNameToString() const;

    std::string packetSenderNameToString() const;

    std::string packetReceiverNameToString() const;

    Serializable::size_type fromBinary(
        const Serializable::container_type& buffer);

    Serializable::container_type toBinary() const;

    static const size_t SIZE =
        sizeof(type) + sizeof(tokenStatus) + sizeof(originalSenderName) +
        sizeof(packetSenderName) + sizeof(packetReceiverName) + sizeof(dataSize);
  };
#pragma pack(pop)

#pragma pack(push, 1)
  class RegisterSubheader : public Serializable {
   public:
    // New host data
    Ip4 ip;
    unsigned short port;

   public:
    virtual ~RegisterSubheader() = default;

    // New host neighbor name
    char neighborToDisconnectName[UserIdentifierNameSize];

    void setNeighborToDisconnectName(const std::string& str);

    std::string neighborToDisconnectNameToString() const;

    Serializable::size_type fromBinary(const container_type& buffer);

    container_type toBinary() const;

    static const size_t SIZE = Ip4::SIZE + sizeof(port);
  };
#pragma pack(pop)

 private:
  Header header;
  std::vector<unsigned char> data;

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

  const std::vector<unsigned char>& getData() const;

  void setData(const std::vector<unsigned char>& value);

  Serializable::size_type fromBinary(
      const Serializable::container_type& buffer) noexcept(false);
  Serializable::container_type toBinary() const;
};

#endif  // TOKENRINGPACKET_H
