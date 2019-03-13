#include <cstring>

#include <atomic>
#include <csignal>
#include <iomanip>
#include <iostream>

#include "programarguments.h"
#include "quitstatusobserver.h"
#include "socket.h"
#include "tokenringdispatcher.h"
#include "tokenringpacket.h"

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

  std::string protocolString =
      (args.getProtocol() == Protocol::TCP
           ? "TCP"
           : (args.getProtocol() == Protocol::UDP ? "UDP" : "Unknown"));

  std::cout << "Program arguments:" << std::endl
            << "UserId: " << args.getUserIdentifier() << std::endl
            << "Port: " << args.getPort() << std::endl
            << "NeighborIp: " << to_string(args.getNeighborIp()) << std::endl
            << "NeighborPort: " << args.getNeighborPort() << std::endl
            << "HasToken: " << std::boolalpha << args.getHasToken() << std::endl
            << "Protocol: " << protocolString << std::endl;

  // Register quit handler
  std::signal(SIGINT, quitStatusObserverHandler);

  // Ignore SIGPIPE
  std::signal(SIGPIPE, SIG_IGN);

  TokenRingDispatcher dispatcher{args};
  dispatcher.run();

  return 0;
}
