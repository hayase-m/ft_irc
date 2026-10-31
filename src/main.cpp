#include "Transport.hpp"

#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

namespace {
unsigned short ParsePort(const std::string &text) {
  if (text.empty() || text.find_first_not_of("0123456789") != std::string::npos)
    throw std::invalid_argument("port must contain only digits");
  std::istringstream stream(text);
  unsigned long port;
  if (!(stream >> port) || port == 0 || port > 65535)
    throw std::invalid_argument("port must be between 1 and 65535");
  return static_cast<unsigned short>(port);
}
} // namespace

int main(int argc, char **argv) {
  if (argc != 3) {
    std::cerr << "Usage: " << argv[0] << " <port> <password>" << std::endl;
    return 1;
  }
  try {
    Transport transport(ParsePort(argv[1]));
    while (true) {
      const std::vector<TransportEvent> events = transport.WaitForEvents();
      // テスト用print
      for (std::size_t i = 0; i < events.size(); ++i)
        std::cout << "connected client_id=" << events[i].client_id << std::endl;
    }
  } catch (const std::exception &e) {
    std::cerr << "ircserv: " << e.what() << std::endl;
    return 1;
  }
}
