#include <cstring>

#include <atomic>
#include <csignal>
#include <iomanip>
#include <iostream>

#include "programarguments.h"
#include "socket.h"
#include "tokenringpacket.h"
#include "quitstatusobserver.h"
#include "tokenringdispatcher.h"

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
            << "NeighborPort: " << args.getNeighborPort() << std::endl
            << "HasToken: " << std::boolalpha << args.getHasToken() << std::endl
            << std::endl;

  // Register quit handler
  std::signal(SIGINT, quitStatusObserverHandler);

  TokenRingDispatcher dispatcher{args};
  dispatcher.run();

  return 0;
}
