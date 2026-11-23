#include "IrcServer.hpp"
#include "ClientState.hpp"
#include "IrcMessage.hpp"

#include <cstddef>
#include <new>
#include <utility>

IrcServer::IrcServer(const std::string &password)
    : clients_(), registration_(password, clients_), channels_(clients_),
      commands_() {
  AddCommand("PASS", &IrcServer::HandleRegistration, false);
  AddCommand("NICK", &IrcServer::HandleRegistration, false);
  AddCommand("USER", &IrcServer::HandleRegistration, false);
  AddCommand("JOIN", &IrcServer::HandleJoin, true);
}

void IrcServer::AddCommand(const std::string &name, CommandHandler handler,
                           bool requires_registration) {
  const Command command = {handler, requires_registration};
  commands_.insert(std::make_pair(name, command));
}

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

  const Command *command = FindCommand(message.command);

  if (command != NULL && !command->requires_registration)
    return (this->*command->handler)(client_id, message, outgoing_messages);

  const std::string target = client->nickname.empty() ? "*" : client->nickname;
  if (!client->registered) {
    outgoing_messages.push_back(OutgoingMessage(
        client_id, ":ircserv 451 " + target + " :You have not registered"));
    return true;
  }
  if (command == NULL) {
    outgoing_messages.push_back(
        OutgoingMessage(client_id, ":ircserv 421 " + target + " " +
                                       message.command + " :Unknown command"));
    return true;
  }
  return (this->*command->handler)(client_id, message, outgoing_messages);
}

const IrcServer::Command *IrcServer::FindCommand(const std::string &name) const {
  const CommandMap::const_iterator command = commands_.find(name);

  if (command == commands_.end())
    return NULL;

  return &command->second;
}

bool IrcServer::HandleRegistration(
    ClientId client_id, const IrcMessage &message,
    std::vector<OutgoingMessage> &outgoing_messages) {
  return registration_.HandleMessage(client_id, message, outgoing_messages);
}

bool IrcServer::HandleJoin(ClientId client_id, const IrcMessage &message,
                           std::vector<OutgoingMessage> &outgoing_messages) {
  return channels_.HandleJoin(client_id, message, outgoing_messages);
}

void IrcServer::HandleDisconnected(ClientId client_id) {
  clients_.RemoveClient(client_id);
}
