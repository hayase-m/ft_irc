#ifndef IRCSERVER_HPP
#define IRCSERVER_HPP

#include "ClientId.hpp"
#include "OutgoingMessage.hpp"
#include "Registration.hpp"

#include <string>
#include <vector>

class IrcServer {
public:
  explicit IrcServer(const std::string &password);

  void HandleConnected(ClientId client_id,
                       std::vector<ClientId> &client_ids_to_disconnect);
  void HandleLine(ClientId client_id, const std::string &line,
                  std::vector<OutgoingMessage> &outgoing_messages,
                  std::vector<ClientId> &client_ids_to_disconnect);
  void HandleDisconnected(ClientId client_id);

private:
  Registration registration_;

  bool ProcessLine(ClientId client_id, const std::string &line,
                   std::vector<OutgoingMessage> &outgoing_messages);

  IrcServer(const IrcServer &other);
  IrcServer &operator=(const IrcServer &other);
};

#endif
