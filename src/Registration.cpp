#include "Registration.hpp"
#include "ClientId.hpp"
#include "ClientState.hpp"
#include "Clients.hpp"
#include "IrcMessage.hpp"
#include "OutgoingMessage.hpp"

#include <cstddef>
#include <new>
#include <vector>

namespace {

bool IsValidNickname(const std::string &nickname) {
  const std::string::size_type kMaxNicknameLength = 9;

  if (nickname.empty() || nickname.size() > kMaxNicknameLength)
    return false;

  const std::string first_characters = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                       "abcdefghijklmnopqrstuvwxyz"
                                       "[]\\`_^{|}";

  const std::string all_characters = first_characters + "0123456789-";

  return first_characters.find(nickname[0]) != std::string::npos &&
         nickname.find_first_not_of(all_characters) == std::string::npos;
}
} // namespace

Registration::Registration(const std::string &server_password, Clients &clients)
    : server_password_(server_password), clients_(clients) {}

ConnectionAction
Registration::HandleMessage(ClientId client_id, const IrcMessage &message,
                            std::vector<OutgoingMessage> &replies) {
  ClientState *client = clients_.FindClient(client_id);
  ConnectionAction action = kKeepOpen;

  if (client == NULL ||
      (message.command != "PASS" && message.command != "NICK" &&
       message.command != "USER") ||
      (message.command == "NICK" && client->registered)) {
    replies.clear();
    return kKeepOpen;
  }

  try {
    if (message.command == "PASS")
      HandlePass(client_id, *client, message, replies);
    else if (message.command == "NICK")
      HandleNick(client_id, *client, message, replies);
    else
      HandleUser(client_id, *client, message, replies);

    if (replies.empty())
      action = TryCompleteRegistration(client_id, *client, replies);
  } catch (const std::bad_alloc &) {
    return kCloseNow;
  }
  return action;
}

void Registration::HandlePass(ClientId client_id, ClientState &client,
                              const IrcMessage &message,
                              std::vector<OutgoingMessage> &replies) {
  const std::string target = client.nickname.empty() ? "*" : client.nickname;

  if (client.registered) {
    replies.push_back(OutgoingMessage(
        client_id, ":ircserv 462 " + target + " :You may not reregister"));
    return;
  }
  if (message.params.empty()) {
    replies.push_back(OutgoingMessage(
        client_id, ":ircserv 461 " + target + " PASS :Not enough parameters"));
    return;
  }
  client.has_connection_password = true;
  client.connection_password = message.params[0];
}

void Registration::HandleNick(ClientId client_id, ClientState &client,
                              const IrcMessage &message,
                              std::vector<OutgoingMessage> &replies) {
  const std::string target = client.nickname.empty() ? "*" : client.nickname;

  if (message.params.empty() || message.params[0].empty()) {
    replies.push_back(OutgoingMessage(client_id, ":ircserv 431 " + target +
                                                     " :No nickname given"));
    return;
  }

  const std::string &nickname = message.params[0];

  if (!IsValidNickname(nickname)) {
    const bool fits_parameter =
        nickname[0] != ':' &&
        nickname.find_first_of(" ") == std::string::npos &&
        nickname.find('\0') == std::string::npos;
    const std::string rejected_nickname = fits_parameter ? nickname : "*";

    replies.push_back(OutgoingMessage(client_id, ":ircserv 432 " + target +
                                                     " " + rejected_nickname +
                                                     " :Erroneous nickname"));

    return;
  }

  ClientId owner_id;

  if (clients_.FindClientIdByNickname(nickname, owner_id) &&
      owner_id != client_id) {
    replies.push_back(
        OutgoingMessage(client_id, ":ircserv 433 " + target + " " + nickname +
                                       " :Nickname is already in use"));
    return;
  }
  client.nickname = nickname;
}

void Registration::HandleUser(ClientId client_id, ClientState &client,
                              const IrcMessage &message,
                              std::vector<OutgoingMessage> &replies) {
  const std::string target = client.nickname.empty() ? "*" : client.nickname;

  if (client.registered) {
    replies.push_back(OutgoingMessage(
        client_id, ":ircserv 462 " + target + " :You may not reregister"));
    return;
  }

  if (message.params.size() < 4 || message.params[0].empty()) {
    replies.push_back(OutgoingMessage(
        client_id, ":ircserv 461 " + target + " USER :Not enough parameters"));
    return;
  }

  const std::string &username = message.params[0];
  const std::string &realname = message.params[3];

  client.username = username;
  client.realname = realname;
}

ConnectionAction
Registration::TryCompleteRegistration(ClientId client_id, ClientState &client,
                                      std::vector<OutgoingMessage> &replies) {
  if (client.registered || client.nickname.empty() || client.username.empty())
    return kKeepOpen;

  if (!client.has_connection_password ||
      client.connection_password != server_password_) {
    replies.push_back(OutgoingMessage(
        client_id, ":ircserv 464 " + client.nickname + " :Password incorrect"));
    replies.push_back(OutgoingMessage(client_id, "ERROR :Closing connection"));
    return kCloseAfterFlush;
  }
  client.authenticated = true;

  replies.push_back(
      OutgoingMessage(client_id, ":ircserv 001 " + client.nickname +
                                     " :Welcome to the IRC network"));
  client.registered = true;
  return kKeepOpen;
}
