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

void TryCompleteRegistration(ClientId client_id, ClientState &candidate,
                             std::vector<OutgoingMessage> &replies) {
  if (candidate.registered || !candidate.authenticated ||
      candidate.nickname.empty() || candidate.username.empty())
    return;

  replies.push_back(OutgoingMessage(client_id, ":ircserv 001 " +
                                                   candidate.nickname +
                                                   " :Welcome to the IRC network"));

  candidate.registered = true;
}

} // namespace

Registration::Registration(const std::string &server_password, Clients &clients)
    : server_password_(server_password), clients_(clients) {}

bool Registration::HandleMessage(ClientId client_id, const IrcMessage &message,
                                 std::vector<OutgoingMessage> &replies) {
  ClientState *client = clients_.FindClient(client_id);

  if (client == NULL ||
      (message.command != "PASS" && message.command != "NICK" &&
       message.command != "USER") ||
      (message.command == "NICK" && client->registered)) {
    replies.clear();
    return true;
  }

  try {
    ClientState candidate = *client;
    std::vector<OutgoingMessage> pending;

    if (message.command == "PASS")
      HandlePass(client_id, candidate, message, pending);
    else if (message.command == "NICK")
      HandleNick(client_id, candidate, message, pending);
    else
      HandleUser(client_id, candidate, message, pending);

    if (pending.empty())
      TryCompleteRegistration(client_id, candidate, pending);

    ClientState &current = *client;
    current.nickname.swap(candidate.nickname);
    current.username.swap(candidate.username);
    current.realname.swap(candidate.realname);
    current.authenticated = candidate.authenticated;
    current.registered = candidate.registered;
    replies.swap(pending);
  } catch (const std::bad_alloc &) {
    return false;
  }
  return true;
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
  if (message.params[0] != server_password_) {
    replies.push_back(OutgoingMessage(
        client_id, ":ircserv 464 " + target + " :Password incorrect"));
    return;
  }

  client.authenticated = true;
}

void Registration::HandleNick(ClientId client_id, ClientState &client,
                              const IrcMessage &message,
                              std::vector<OutgoingMessage> &replies) {
  const std::string target = client.nickname.empty() ? "*" : client.nickname;

  if (message.params.empty() || message.params[0].empty()) {
    replies.push_back(OutgoingMessage(
        client_id, ":ircserv 431 " + target + " :No nickname given"));
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
    replies.push_back(OutgoingMessage(client_id,
                                      ":ircserv 433 " + target + " " +
                                          nickname +
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
