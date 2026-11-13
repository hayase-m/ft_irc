#include "Registration.hpp"
#include "ClientState.hpp"
#include "IrcMessage.hpp"
#include "Transport.hpp"

#include <cstddef>
#include <new>
#include <utility>
#include <vector>

Registration::Registration(const std::string &server_password)
    : server_password_(server_password) {}

bool Registration::AddClient(ClientId client_id) {
  try {
    return clients_.insert(std::make_pair(client_id, ClientState())).second;
  } catch (const std::bad_alloc) {
    return false;
  }
}

void Registration::RemoveClient(ClientId client_id) {
  clients_.erase(client_id);
}

const ClientState *Registration::FindClient(ClientId client_id) const {
  std::map<ClientId, ClientState>::const_iterator client =
      clients_.find(client_id);

  if (client == clients_.end())
    return NULL;

  return &client->second;
}

bool Registration::HandleMessage(ClientId client_id, const IrcMessage &message,
                                 std::vector<std::string> &replies) {
  std::map<ClientId, ClientState>::iterator client = clients_.find(client_id);

  if (client == clients_.end() ||
      (message.command != "PASS" && message.command != "NICK" &&
       message.command != "USER") ||
      (message.command == "NICK" && client->second.registered)) {
    replies.clear();
    return true;
  }

  try {
    {
      ClientState candidate = client->second;
      std::vector<std::string> pending;

      if (message.command == "PASS")
        HandlePass(candidate, message, pending);
      else if (message.command == "NICK")
        HandleNick(client_id, candidate, message, pending);
      else
        HandleUser(candidate, message, pending);

      if (pending.empty())
        TryCompleteRegistration(candidate, pending);

      ClientState &current = client->second;
      current.nickname.swap(candidate.nickname);
      current.username.swap(candidate.username);
      current.realname.swap(candidate.realname);
      current.authenticated = candidate.authenticated;
      current.registered = candidate.registered;
      replies.swap(pending);
    }
  } catch (const std::bad_alloc &) {
    return false;
  }
  return true;
}

namespace {

std::string NicknameKey(const std::string &nickname) {
  const std::string upper = "ABCDEFGHIJKLMNOPQRSTUVWXYZ[]\\^";
  const std::string lower = "abcdefghijklmnopqrstuvwxyz{}|~";
  std::string key = nickname;

  for (std::string::size_type i = 0; i < key.size(); ++i) {
    const std::string::size_type index = upper.find(key[i]);

    if (index != std::string::npos)
      key[i] = lower[index];
  }
  return key;
}
} // namespace

bool Registration::IsValidNickname(const std::string &nickname) {
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

void Registration::HandlePass(ClientState &client, const IrcMessage &message,
                              std::vector<std::string> &replies) {
  const std::string target = client.nickname.empty() ? "*" : client.nickname;

  if (client.registered) {
    replies.push_back(":ircserv 462 " + target +
                      " :You may not reregister\r\n");
    return;
  }
  if (message.params.empty()) {
    replies.push_back(":ircserv 461 " + target +
                      " PASS :Not enough parameters\r\n");
    return;
  }
  if (message.params[0] != server_password_) {
    replies.push_back(":ircserv 464 " + target + " :Password incorrect\r\n");
    return;
  }

  client.authenticated = true;
}

void Registration::HandleNick(ClientId client_id, ClientState &client,
                              const IrcMessage &message,
                              std::vector<std::string> &replies) {
  const std::string target = client.nickname.empty() ? "*" : client.nickname;

  if (message.params.empty() || message.params[0].empty()) {
    replies.push_back(":ircserv 431 " + target + " :No nickname given \r\n");
    return;
  }

  const std::string &nickname = message.params[0];

  if (!IsValidNickname(nickname)) {
    const bool fits_parameter =
        nickname[0] != ':' &&
        nickname.find_first_of(" \r\n") == std::string::npos &&
        nickname.find('\0') == std::string::npos;
    const std::string rejected_nickname = fits_parameter ? nickname : "*";

    replies.push_back(":ircserv 432 " + target + " " + rejected_nickname +
                      " :Invalide nickname\r\n");

    return;
  }

  const std::string nickname_key = NicknameKey(nickname);

  for (std::map<ClientId, ClientState>::const_iterator other = clients_.begin();
       other != clients_.end(); ++other) {
    if (other->first != client_id &&
        NicknameKey(other->second.nickname) == nickname_key) {
      replies.push_back(":ircserv 433 " + target + " " + nickname +
                        " :Nickname is already in use\r\n");
      return;
    }
  }
  std::string new_nickname(nickname);
  client.nickname.swap(new_nickname);
}

void Registration::HandleUser(ClientState &client, const IrcMessage &message,
                              std::vector<std::string> &replies) {
  const std::string target = client.nickname.empty() ? "*" : client.nickname;

  if (client.registered) {
    replies.push_back(":ircserv 462 " + target +
                      " :You may not reregister\r\n");
    return;
  }

  if (message.params.size() < 4 || message.params[0].empty()) {
    replies.push_back(":ircserv 461 " + target +
                      " USER :Not enough parameters\r\n");
    return;
  }

  std::string new_username(message.params[0]);
  std::string new_realname(message.params[3]);

  client.username.swap(new_username);
  client.realname.swap(new_realname);
}

void Registration::TryCompleteRegistration(ClientState &candidate,
                                           std::vector<std::string> &replies) {
  if (candidate.registered || !candidate.authenticated ||
      candidate.nickname.empty() || candidate.username.empty())
    return;

  replies.push_back(":ircserv 001 " + candidate.nickname +
                    " :Welcome to the IRC network\r\n");

  candidate.registered = true;
}
