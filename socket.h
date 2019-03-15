#ifndef SOCKET_H
#define SOCKET_H

#include <sys/socket.h>
#include <sys/types.h>

#include <chrono>
#include <stdexcept>
#include <vector>

#include "ip4.h"
#include "protocol.h"

using SocketException = std::runtime_error;

using SocketInvalidProtocolException = SocketException;

using SocketCreationFailedException = SocketException;

using SocketBindFailedException = SocketException;

using SocketConnectFailedException = SocketException;

using SocketListenFailedException = SocketException;

using SocketSelectException = SocketException;

using SocketSelectFailedException = SocketSelectException;

using SocketSelectTimeoutException = SocketSelectException;

using SocketAcceptFailedException = SocketException;

using SocketSendingFailedException = SocketException;

using SocketReceivingFailedException = SocketException;

class Socket {
 public:
  static const int bufferSize = 1024;

  using IpAndPortPair = std::pair<Ip4, unsigned short>;

 private:
  Protocol protocol = Protocol::NONE;
  int socketDescriptor;

  Ip4 connectionIp{0};
  unsigned short connectionPort{0};

 private:
  explicit Socket(int descriptor) noexcept(false);

  explicit Socket(int descriptor, in_addr address,
                  unsigned short port) noexcept(false);

 public:
  explicit Socket(Protocol protocol) noexcept(false);

  Socket(const Socket&);
  explicit Socket(Socket&&);

  Socket& operator=(const Socket&);
  Socket& operator=(Socket&&);

  ~Socket();

  void bind(const Ip4& ip, unsigned short port) noexcept(false);

  void connect(const Ip4& ip, unsigned short port) noexcept(false);

  void listen(int backlog = 50) noexcept(false);

  /**
   * Select without timeout
   */
  Socket select() noexcept(false);

  Socket select(const std::chrono::milliseconds& timeout) noexcept(false);

  Socket select(const std::chrono::seconds& timeout) noexcept(false);

  Socket accept() noexcept(false);

  void send(const std::vector<unsigned char>& data) noexcept(false);

  void sendTo(const std::vector<unsigned char>& data, const Ip4& destination,
              unsigned short port) noexcept(false);

  std::vector<unsigned char> receive(size_t bufferSize = 0) noexcept(false);

  std::pair<IpAndPortPair, std::vector<unsigned char>> receiveFrom(size_t bufferSize = 0) noexcept(
      false);

  void disconnect() noexcept;

  void close() noexcept;

  Ip4 getConnectionIp() const;

  unsigned short getConnectionPort() const;

  Ip4 getIp() const;

  unsigned short getPort() const;

  bool hasAnyDataToRead() const;

  int dataToRead() const;

 private:
  Socket select(struct timeval* tv) noexcept(false);

  void getProtocolTypeFromSocketOpts(int descriptor);

  void getIpAndPortFromSocket(int descriptor);
};

#endif  // SOCKET_H
