#ifndef IRCSERVER_HPP
#define IRCSERVER_HPP

#include "ClientId.hpp"
#include "Clients.hpp"
#include "ConnectionAction.hpp"
#include "OutgoingMessage.hpp"
#include "Registration.hpp"

#include <string>
#include <vector>

class IrcServer {
public:
  explicit IrcServer(const std::string &password);

  ConnectionAction HandleConnected(ClientId client_id);
  ConnectionAction HandleLine(ClientId client_id, const std::string &line,
                              std::vector<OutgoingMessage> &outgoing_messages);
  void HandleDisconnected(ClientId client_id);

private:
  Clients clients_;
  Registration registration_;

  IrcServer(const IrcServer &other);
  IrcServer &operator=(const IrcServer &other);
};

#endif
