#include "IrcServer.hpp"
#include "ClientState.hpp"
#include "IrcMessage.hpp"

namespace {
bool IsRegistrationCommand(const std::string &command) {
  return command == "PASS" || command == "NICK" || command == "USER";
}
} // namespace

IrcServer::IrcServer(const std::string &password)
    : clients_(), registration_(password, clients_) {}

ConnectionAction IrcServer::HandleConnected(ClientId client_id) {
  return clients_.AddClient(client_id) ? kKeepOpen : kCloseNow;
}

ConnectionAction IrcServer::HandleLine(
    ClientId client_id, const std::string &line,
    std::vector<OutgoingMessage> &outgoing_messages) {
  const ClientState *client = clients_.FindClient(client_id);
  IrcMessage message;
  if (client == NULL || !parseMessage(line, message))
    return kKeepOpen;
  if (IsRegistrationCommand(message.command))
    return registration_.HandleMessage(client_id, message, outgoing_messages);
  const std::string target = client->nickname.empty() ? "*" : client->nickname;
  if (!client->registered)
    outgoing_messages.push_back(OutgoingMessage(
        client_id, ":ircserv 451 " + target + " :You have not registered"));
  else
    outgoing_messages.push_back(
        OutgoingMessage(client_id, ":ircserv 421 " + target + " " +
                                       message.command + " :Unknown command"));
  return kKeepOpen;
}

void IrcServer::HandleDisconnected(ClientId client_id) {
  clients_.RemoveClient(client_id);
}
