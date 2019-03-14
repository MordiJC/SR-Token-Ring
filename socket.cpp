#include "socket.h"

#include <sys/ioctl.h>
#include <unistd.h>

#include <cstring>
#include <iostream>

void Socket::getProtocolTypeFromSocketOpts(int descriptor) {
  int socketTypeOpt = 0;
  unsigned int socketTypeSize = sizeof(socketTypeOpt);
  int socketProtocolOpt = 0;
  unsigned int socketProtocolSize = sizeof(socketProtocolOpt);
  int ret = 0;

  ret = ::getsockopt(descriptor, SOL_SOCKET, SO_TYPE, &socketTypeOpt,
                     &socketTypeSize);

  if (ret == -1) {
    perror("Unable to get socket type opt");
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

void Socket::getIpAndPortPromSocket(int descriptor) {
  int ret = 0;
  struct sockaddr_in socketAddressStruct;
  unsigned int socketAddressStructSize = sizeof(socketAddressStruct);
  ret = ::getpeername(descriptor,
                      reinterpret_cast<struct sockaddr *>(&socketAddressStruct),
                      &socketAddressStructSize);

  if (ret == -1) {
    throw SocketException("Unable to get connection IP and port number");
  }

  connectionIp = Ip4(socketAddressStruct.sin_addr);
  connectionPort = ntohs(socketAddressStruct.sin_port);
}

Ip4 Socket::getConnectionIp() const { return connectionIp; }

unsigned short Socket::getConnectionPort() const { return connectionPort; }

Ip4 Socket::getIp() const {
  struct sockaddr_in sockAddrIn;
  socklen_t len = sizeof(sockAddrIn);
  if (::getsockname(socketDescriptor,
                    reinterpret_cast<struct sockaddr *>(&sockAddrIn),
                    &len) == -1) {
    perror("getsockname");
    throw SocketException("Failed to acquire socket IP");
  } else
    return Ip4(sockAddrIn.sin_addr);
}

unsigned short Socket::getPort() const {
  struct sockaddr_in sockAddrIn;
  socklen_t len = sizeof(sockAddrIn);
  if (::getsockname(socketDescriptor,
                    reinterpret_cast<struct sockaddr *>(&sockAddrIn),
                    &len) == -1) {
    perror("getsockname");
    throw SocketException("Failed to acquire socket port");
  } else
    return ntohs(sockAddrIn.sin_port);
}

bool Socket::hasAnyDataToRead() const {
  int count;
  ioctl(socketDescriptor, FIONREAD, &count);

  return count > 0;
}

int Socket::dataToRead() const {
  int count;
  ioctl(socketDescriptor, FIONREAD, &count);

  return count;
}

Socket::Socket(int descriptor) : socketDescriptor(descriptor) {
  getProtocolTypeFromSocketOpts(descriptor);

  getIpAndPortPromSocket(descriptor);
}

Socket::Socket(int descriptor, in_addr address,
               unsigned short port) noexcept(false)
    : socketDescriptor(descriptor),
      connectionIp(Ip4(address)),
      connectionPort(port) {
  getProtocolTypeFromSocketOpts(descriptor);
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

  if (protocol == Protocol::TCP) {
    setsockopt(socketDescriptor, SOL_SOCKET, SO_REUSEADDR, nullptr, 0);
  }
}

Socket::Socket(const Socket &other)
    : protocol(other.protocol),
      socketDescriptor(other.socketDescriptor),
      connectionIp(other.connectionIp),
      connectionPort(other.connectionPort) {}

Socket::Socket(Socket &&other)
    : protocol(other.protocol),
      socketDescriptor(other.socketDescriptor),
      connectionIp(other.connectionIp),
      connectionPort(other.connectionPort) {}

Socket &Socket::operator=(const Socket &other) {
  protocol = other.protocol;
  socketDescriptor = other.socketDescriptor;
  connectionIp = other.connectionIp;
  connectionPort = other.connectionPort;
  return *this;
}

Socket &Socket::operator=(Socket &&other) {
  protocol = other.protocol;
  socketDescriptor = other.socketDescriptor;
  connectionIp = other.connectionIp;
  connectionPort = other.connectionPort;
  return *this;
}

Socket::~Socket() {}

void Socket::bind(const Ip4 &ip, unsigned short port) noexcept(false) {
  struct sockaddr_in socketAddressStruct;

  std::memset(&socketAddressStruct, 0, sizeof(struct sockaddr_in));

  socketAddressStruct.sin_addr = ip;
  socketAddressStruct.sin_port = htons(port);
  socketAddressStruct.sin_family = AF_INET;

  int ret = ::bind(socketDescriptor,
                   reinterpret_cast<struct sockaddr *>(&socketAddressStruct),
                   sizeof(socketAddressStruct));

  if (ret == -1) {
    perror("Socket binding error");
    throw SocketBindFailedException("Failed to bind to specified ip and port");
  }

  connectionIp = ip;
  connectionPort = port;
}

void Socket::connect(const Ip4 &ip, unsigned short port) {
  struct sockaddr_in socketAddressStruct;

  std::memset(&socketAddressStruct, 0, sizeof(struct sockaddr_in));

  socketAddressStruct.sin_addr = ip;
  socketAddressStruct.sin_port = htons(port);
  socketAddressStruct.sin_family = AF_INET;

  std::cout << "[INFO] Connecting to " << to_string(ip) << ':' << port
            << std::endl;

  int ret = ::connect(socketDescriptor,
                      reinterpret_cast<struct sockaddr *>(&socketAddressStruct),
                      sizeof(socketAddressStruct));

  if (ret == -1) {
    throw SocketConnectFailedException(
        "Failed to connect to specified ip and port");
  }

  connectionIp = ip;
  connectionPort = port;
}

void Socket::listen(int backlog) noexcept(false) {
  if (protocol == Protocol::TCP) {
    int ret = ::listen(socketDescriptor, backlog);

    if (ret == -1) {
      throw SocketListenFailedException("Failed to listen on socket");
    }
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
  int peendingConnections = 0;
  fd_set readfds;

  FD_ZERO(&readfds);
  FD_SET(socketDescriptor, &readfds);

  peendingConnections =
      ::select(socketDescriptor + 1, &readfds, nullptr, nullptr, tv);

  if (peendingConnections == -1) {
    throw SocketSelectFailedException("Failed to select socket");
  } else if (peendingConnections == 0) {
    throw SocketSelectTimeoutException("Socket select timout");
  }

  if (FD_ISSET(socketDescriptor, &readfds)) {
    return accept();
  }

  throw SocketSelectTimeoutException("Socket select general error");
}

Socket Socket::accept() noexcept(false) {
  int descriptor = 0;
  struct sockaddr_in incomingAddress;
  unsigned int incomingAddressSize = sizeof(incomingAddress);

  descriptor = ::accept(socketDescriptor,
                        reinterpret_cast<struct sockaddr *>(&incomingAddress),
                        &incomingAddressSize);

  if (descriptor < 0) {
    throw SocketAcceptFailedException("Failed to accept connection");
  }

  return Socket(descriptor, incomingAddress.sin_addr,
                ntohs(incomingAddress.sin_port));
}

void Socket::send(const std::vector<unsigned char> &data) noexcept(false) {
  ssize_t sentSize = 0;

  sentSize = ::send(socketDescriptor, data.data(), data.size(), 0);

  if (sentSize == -1) {
    perror("Failed to send data");
    throw SocketSendingFailedException("Failed to send data");
  }
}

void Socket::sendTo(const std::vector<unsigned char> &data,
                    const Ip4 &destination,
                    unsigned short port) noexcept(false) {
  ssize_t sentSize = 0;

  struct sockaddr_in destinationAddress;

  ::memset(&destinationAddress, 0, sizeof(destinationAddress));

  destinationAddress.sin_addr = destination;
  destinationAddress.sin_port = htons(port);
  destinationAddress.sin_family = AF_INET;

  sentSize = ::sendto(socketDescriptor, data.data(), data.size(), 0,
                      reinterpret_cast<struct sockaddr *>(&destinationAddress),
                      sizeof(destinationAddress));

  if (sentSize == -1) {
    throw SocketSendingFailedException("Failed to send data to specified host");
  }
}

std::vector<unsigned char> Socket::receive() noexcept(false) {
  std::vector<unsigned char> buffer;
  buffer.resize(bufferSize);
  ssize_t recvSize = 0;

  recvSize = ::recv(socketDescriptor, buffer.data(), bufferSize, 0);

  if (recvSize == -1) {
    perror("Failed to receive data from socket");
    throw SocketReceivingFailedException("Failed to receive data from socket");
  }

  buffer.resize(static_cast<size_t>(recvSize));

  return buffer;
}

std::pair<Socket::IpAndPortPair, std::vector<unsigned char>>
Socket::receiveFrom() noexcept(false) {
  std::vector<unsigned char> buffer(bufferSize);
  ssize_t recvSize = 0;

  struct sockaddr_in sourceAddress;
  socklen_t sourceAddressSize = sizeof(struct sockaddr_in);

  recvSize = ::recvfrom(socketDescriptor, buffer.data(), bufferSize, 0,
                        reinterpret_cast<struct sockaddr *>(&sourceAddress),
                        &sourceAddressSize);

  if (recvSize == -1) {
    throw SocketReceivingFailedException("Failed to receive data from socket");
  }

  std::cout << "[INFO][RECVFROM] Received from "
            << to_string(sourceAddress.sin_addr) << ':'
            << ntohs(sourceAddress.sin_port) << std::endl;

  buffer.resize(static_cast<size_t>(recvSize));

  return std::make_pair(std::make_pair(Ip4(sourceAddress.sin_addr),
                                       ntohs(sourceAddress.sin_port)),
                        buffer);
}

void Socket::disconnect() noexcept {
  ::shutdown(this->socketDescriptor, SHUT_RDWR);
}

void Socket::close() noexcept { ::close(socketDescriptor); }
