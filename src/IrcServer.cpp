#include "IrcServer.hpp"
#include "ClientState.hpp"
#include "IrcMessage.hpp"

#include <new>

namespace {
void AddOutgoingMessage(std::vector<OutgoingMessage> &outgoing_messages,
                        ClientId client_id, const std::string &line) {
  OutgoingMessage outgoing_message;
  outgoing_message.client_id = client_id;
  outgoing_message.line = line;
  outgoing_messages.push_back(outgoing_message);
}

bool IsRegistrationCommand(const std::string &command) {
  return command == "PASS" || command == "NICK" || command == "USER";
}
} // namespace

IrcServer::IrcServer(const std::string &password) : registration_(password) {}

void IrcServer::HandleConnected(
    ClientId client_id, std::vector<ClientId> &client_ids_to_disconnect) {
  if (!registration_.AddClient(client_id))
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
  const ClientState *client = registration_.FindClient(client_id);
  IrcMessage message;
  if (client == NULL || !parseMessage(line, message))
    return true;
  if (IsRegistrationCommand(message.command)) {
    std::vector<std::string> replies;
    if (!registration_.HandleMessage(client_id, message, replies))
      return false;
    for (std::vector<std::string>::size_type i = 0; i < replies.size(); ++i)
      AddOutgoingMessage(outgoing_messages, client_id, replies[i]);
    return true;
  }
  const std::string target = client->nickname.empty() ? "*" : client->nickname;
  if (!client->registered)
    AddOutgoingMessage(outgoing_messages, client_id,
                       ":ircserv 451 " + target + " :You have not registered");
  else
    AddOutgoingMessage(outgoing_messages, client_id,
                       ":ircserv 421 " + target + " " + message.command +
                           " :Unknown command");
  return true;
}

void IrcServer::HandleDisconnected(ClientId client_id) {
  registration_.RemoveClient(client_id);
}
