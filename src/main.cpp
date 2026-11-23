#include "IrcServer.hpp"
#include "OutgoingMessage.hpp"
#include "Transport.hpp"

#include <csignal>
#include <iostream>
#include <new>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

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

void HandleDisconnectedClients(Transport &transport, IrcServer &server) {
  ClientId client_id;
  while (transport.PopDisconnectedClientId(client_id))
    server.HandleDisconnected(client_id);
}

void HandleEvent(Transport &transport, IrcServer &server,
                 const TransportEvent &event) {
  try {
    std::vector<OutgoingMessage> outgoing_messages;
    std::vector<ClientId> client_ids_to_disconnect;
    switch (event.type) {
    case TransportEvent::kConnected:
      server.HandleConnected(event.client_id, client_ids_to_disconnect);
      break;
    case TransportEvent::kLineReceived:
      server.HandleLine(event.client_id, event.line, outgoing_messages,
                        client_ids_to_disconnect);
      break;
    }
    for (std::vector<OutgoingMessage>::size_type i = 0;
         i < outgoing_messages.size(); ++i)
      transport.QueueClientLineToSend(outgoing_messages[i].client_id,
                                      outgoing_messages[i].line);
    for (std::vector<ClientId>::size_type i = 0;
         i < client_ids_to_disconnect.size(); ++i)
      transport.Disconnect(client_ids_to_disconnect[i]);
  } catch (const std::bad_alloc &) {
    transport.Disconnect(event.client_id);
  }
}
} // namespace

int main(int argc, char **argv) {
  if (argc != 3) {
    std::cerr << "Usage: " << argv[0] << " <port> <password>" << std::endl;
    return 1;
  }
  std::signal(SIGPIPE, SIG_IGN);
  try {
    Transport transport(ParsePort(argv[1]));
    IrcServer server(argv[2]);
    while (true) {
      const std::vector<TransportEvent> events = transport.WaitForEvents();
      HandleDisconnectedClients(transport, server);
      for (std::vector<TransportEvent>::size_type i = 0; i < events.size();
           ++i) {
        HandleEvent(transport, server, events[i]);
        HandleDisconnectedClients(transport, server);
      }
    }
  } catch (const std::exception &e) {
    std::cerr << "ircserv: " << e.what() << std::endl;
    return 1;
  }
}
