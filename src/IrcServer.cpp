#include "IrcServer.hpp"
#include "ClientState.hpp"
#include "IrcMessage.hpp"

#include <new>

namespace {
bool IsRegistrationCommand(const std::string &command) {
  return command == "PASS" || command == "NICK" || command == "USER";
}
} // namespace

IrcServer::IrcServer(const std::string &password)
    : clients_(), registration_(password, clients_) {}

void IrcServer::HandleConnected(
    ClientId client_id, std::vector<ClientId> &client_ids_to_disconnect) {
  if (!clients_.AddClient(client_id))
    client_ids_to_disconnect.push_back(client_id);
}

void IrcServer::HandleLine(ClientId client_id, const std::string &line,
                           std::vector<OutgoingMessage> &outgoing_messages,
                           std::vector<ClientId> &client_ids_to_disconnect) {
  try {
    if (ProcessLine(client_id, line, outgoing_messages))
      return;
  } catch (const std::bad_alloc &) {
  }
  outgoing_messages.clear();
  client_ids_to_disconnect.push_back(client_id);
}

bool IrcServer::ProcessLine(ClientId client_id, const std::string &line,
                            std::vector<OutgoingMessage> &outgoing_messages) {
  const ClientState *client = clients_.FindClient(client_id);
  IrcMessage message;
  if (client == NULL || !parseMessage(line, message))
    return true;
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
  return true;
}

void IrcServer::HandleDisconnected(ClientId client_id) {
  clients_.RemoveClient(client_id);
}
