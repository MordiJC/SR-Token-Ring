#ifndef SOCKET_H
#define SOCKET_H

#include <sys/socket.h>
#include <sys/types.h>

#include <chrono>
#include <stdexcept>

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

class Socket {
 public:
  enum class ConnectionStatus : unsigned int {
    NONE = 0u,
    NORMAL,
    BOUND,
    CONNECTED,

    NUMBER  //! Number of states
  };

 private:
  Protocol protocol = Protocol::NONE;
  int socketDescriptor;

  Ip4 connectionIp;
  unsigned short connectionPort;

  ConnectionStatus connectionStatus = ConnectionStatus::NORMAL;

 private:
  explicit Socket(int descriptor, bool peer = false) noexcept(false);

 public:
  explicit Socket(Protocol protocol) noexcept(false);

  Socket(const Socket&) = delete;
  Socket(Socket&&) = default;

  Socket& operator=(const Socket&) = delete;
  Socket& operator=(Socket&&) = default;

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

 private:
  Socket select(struct timeval* tv) noexcept(false);
  void getProtocolTypeFromSocketOpts(int descriptor);
};

#endif  // SOCKET_H
