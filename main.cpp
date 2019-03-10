#include <cstring>

#include <atomic>
#include <csignal>
#include <iomanip>
#include <iostream>

#include "programarguments.h"
#include "socket.h"
#include "tokenringpacket.h"
#include "quitstatusobserver.h"

using namespace std;

int main(int argc, char *argv[]) {
  ProgramArguments args(std::vector<const char *>(argv + 1, argv + argc));

  try {
    args.parse();
  } catch (const ProgramArgumentsException &ex) {
    std::cerr << "Program exception: " << ex.what() << std::endl
              << "Terminating" << std::endl;
    std::exit(1);
  }

  std::cout << "Program arguments:" << std::endl
            << "UserId: " << args.getUserIdentifier() << std::endl
            << "Port: " << args.getPort() << std::endl
            << "NeighborIp: " << args.getNeighborIp().to_string() << std::endl
            << "NeighborPort: " << args.getPort() << std::endl
            << "HasToken: " << std::boolalpha << args.getHasToken() << std::endl
            << std::endl;

  // Register quit handler
  std::signal(SIGINT, quitStatusObserverHandler);

  Socket subscriptionSocket{args.getProtocol()};
  Socket outSocket{args.getProtocol()};

  subscriptionSocket.bind(Ip4("127.0.0.1"), args.getPort());

  if (args.getProtocol() == Protocol::TCP) {
    outSocket.connect(Ip4("127.0.0.1"), args.getNeighborPort());

    TokenRingPacket packet;

    TokenRingPacket::Header header;

    std::memset(&header, 0, sizeof(header));

    header.type = TokenRingPacket::PacketType::REGISTER;
    header.tokenStatus = 0;

    std::memcpy(header.originalSenderName, args.getUserIdentifier().c_str(),
                std::min(sizeof(header.originalSenderName) - 1,
                         args.getUserIdentifier().size()));

    std::memcpy(header.packetSenderName, args.getUserIdentifier().c_str(),
                std::min(sizeof(header.packetSenderName) - 1,
                         args.getUserIdentifier().size()));
  }

  return 0;
}
