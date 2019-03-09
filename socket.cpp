#include "socket.h"

#include <unistd.h>

#include <cstring>

void Socket::getProtocolTypeFromSocketOpts(int descriptor) {
  int socketTypeOpt = 0;
  unsigned int socketTypeSize = sizeof(socketTypeOpt);
  int socketProtocolOpt = 0;
  unsigned int socketProtocolSize = sizeof(socketProtocolOpt);
  int ret = 0;

  ret = ::getsockopt(descriptor, SOL_SOCKET, SO_TYPE, &socketTypeOpt,
                     &socketTypeSize);

  if (ret == -1) {
    throw SocketException("Unable to get socket type opt");
  }

  ret = ::getsockopt(descriptor, SOL_SOCKET, SO_PROTOCOL, &socketProtocolOpt,
                     &socketProtocolSize);

  if (ret == -1) {
    throw SocketException("Unable to get socket protocol opt");
  }

  if (socketTypeOpt == SOCK_STREAM && socketProtocolOpt == IPPROTO_TCP) {
    protocol = Protocol::TCP;
  } else if (socketTypeOpt == SOCK_DGRAM && socketProtocolOpt == IPPROTO_UDP) {
    protocol = Protocol::UDP;
  } else {
    throw SocketInvalidProtocolException("Unsupported socket type");
  }
}

Socket::Socket(int descriptor, bool peer) : socketDescriptor(descriptor) {
  getProtocolTypeFromSocketOpts(descriptor);

  int ret = 0;
  struct sockaddr_in socketAddressStruct;
  unsigned int socketAddressStructSize = sizeof(socketAddressStruct);
  if (peer) {
    ret = ::getpeername(
        descriptor, reinterpret_cast<struct sockaddr *>(&socketAddressStruct),
        &socketAddressStructSize);

    if (ret == -1) {
      throw SocketException("Unable to get connection IP and port number");
    }
  } else {
    ret = ::getsockname(
        descriptor, reinterpret_cast<struct sockaddr *>(&socketAddressStruct),
        &socketAddressStructSize);

    if (ret == -1) {
      throw SocketException("Unable to get connection IP and port number");
    }
  }

  connectionIp = Ip4(socketAddressStruct.sin_addr);
  connectionPort = socketAddressStruct.sin_port;
}

Socket::Socket(Protocol protocol) noexcept(false) : protocol(protocol) {
  int socket_type = 0;
  int socket_protocol = 0;

  if (protocol == Protocol::TCP) {
    socket_type = SOCK_STREAM;
    socket_protocol = IPPROTO_TCP;
  } else if (protocol == Protocol::UDP) {
    socket_type = SOCK_DGRAM;
    socket_protocol = IPPROTO_UDP;
  } else {
    throw SocketInvalidProtocolException("Invalid protocol type");
  }

  socketDescriptor = socket(AF_INET, socket_type, socket_protocol);

  if (socketDescriptor == -1) {
    throw SocketCreationFailedException("Failed to create socket");
  }

  connectionStatus = ConnectionStatus::NORMAL;
}

Socket::~Socket() { ::close(socketDescriptor); }

void Socket::bind(const Ip4 &ip, unsigned short port) noexcept(false) {
  struct sockaddr_in socketAddressStruct;

  std::memset(&socketAddressStruct, 0, sizeof(struct sockaddr_in));

  socketAddressStruct.sin_addr = ip.getAddress();
  socketAddressStruct.sin_port = port;
  socketAddressStruct.sin_family = AF_INET;

  int ret = ::bind(socketDescriptor,
                   reinterpret_cast<struct sockaddr *>(&socketAddressStruct),
                   sizeof(socketAddressStruct));

  if (ret == -1) {
    throw SocketBindFailedException("Failed to bind to specified ip and port");
  }

  connectionIp = ip;
  connectionPort = port;

  connectionStatus = ConnectionStatus::BOUND;
}

void Socket::connect(const Ip4 &ip, unsigned short port) {
  struct sockaddr_in socketAddressStruct;

  std::memset(&socketAddressStruct, 0, sizeof(struct sockaddr_in));

  socketAddressStruct.sin_addr = ip.getAddress();
  socketAddressStruct.sin_port = port;
  socketAddressStruct.sin_family = AF_INET;

  int ret = ::connect(socketDescriptor,
                      reinterpret_cast<struct sockaddr *>(&socketAddressStruct),
                      sizeof(socketAddressStruct));

  if (ret == -1) {
    throw SocketConnectFailedException(
        "Failed to connect to specified ip and port");
  }

  connectionIp = ip;
  connectionPort = port;

  connectionStatus = ConnectionStatus::CONNECTED;
}

void Socket::listen(int backlog) noexcept(false) {
  int ret = ::listen(socketDescriptor, backlog);

  if (ret == -1) {
    throw SocketListenFailedException("Failed to listen on socket");
  }
}

Socket Socket::select() noexcept(false) { return select(nullptr); }

Socket Socket::select(const std::chrono::milliseconds &timeout) noexcept(
    false) {
  struct timeval tv;

  tv.tv_sec = static_cast<time_t>(
      std::chrono::duration_cast<std::chrono::seconds>(timeout).count());
  tv.tv_usec = (timeout - std::chrono::duration_cast<std::chrono::milliseconds>(
                              std::chrono::seconds(tv.tv_sec)))
                   .count();

  return select(&tv);
}

Socket Socket::select(const std::chrono::seconds &timeout) noexcept(false) {
  return select(std::chrono::duration_cast<std::chrono::milliseconds>(timeout));
}

Socket Socket::select(struct timeval *tv) noexcept(false) {
  int selectedDescriptor = 0;
  fd_set readfds;

  FD_ZERO(&readfds);
  FD_SET(socketDescriptor, &readfds);

  selectedDescriptor =
      ::select(socketDescriptor, &readfds, nullptr, nullptr, tv);

  if (selectedDescriptor == -1) {
    if (tv != nullptr) {
      throw SocketSelectFailedException("Failed to select socket");
    } else {
      throw SocketSelectTimeoutException("Socket select timout");
    }
  }

  return Socket(selectedDescriptor, true);
}
