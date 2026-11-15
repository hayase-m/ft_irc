#include "Channels.hpp"
#include "IrcMessage.hpp"
#include "Registration.hpp"

#include <algorithm>
#include <new>
#include <utility>

namespace {

const std::string::size_type kMaxChannelLength = 50;
const std::string::size_type kMaxLineLength = 512;

bool IsValidChannelName(const std::string &name) {
  return name.size() >= 2 && name.size() <= kMaxChannelLength &&
         (name[0] == '#' || name[0] == '&') &&
         name.find_first_of(" ,:\a\r\n") == std::string::npos &&
         name.find('\0') == std::string::npos;
}

std::string ChannelKey(const std::string &name) {
  const std::string upper = "ABCDEFGHIJKLMNOPQRSTUVWXYZ[]\\^";
  const std::string lower = "abcdefghijklmnopqrstuvwxyz{}|~";
  std::string key = name;
  for (std::string::size_type i = 0; i < key.size(); ++i) {
    const std::string::size_type index = upper.find(key[i]);
    if (index != std::string::npos)
      key[i] = lower[index];
  }
  return key;
}

void AddReply(std::vector<ChannelReply> &replies, ClientId client_id,
              const std::string &message) {
  const ChannelReply reply = {client_id, message};
  replies.push_back(reply);
}

void AddName(std::vector<ChannelReply> &replies, ClientId client_id,
             const std::string &header, std::string &line,
             const std::string &name) {
  const std::string::size_type separator = line.size() == header.size() ? 0 : 1;
  if (line.size() + separator + name.size() + 2 > kMaxLineLength) {
    AddReply(replies, client_id, line + "\r\n");
    line = header;
  }
  if (line.size() != header.size())
    line += ' ';
  line += name;
}

} // namespace

bool Channels::HandleJoin(const Registration &registration, ClientId client_id,
                          const IrcMessage &message,
                          std::vector<ChannelReply> &replies) {
  std::vector<ChannelReply> pending;
  std::vector<ChannelMap::iterator> joined;

  try {
    const ClientState *client = registration.FindClient(client_id);
    if (message.command == "JOIN" && client != NULL) {
      const std::string target =
          client->nickname.empty() ? "*" : client->nickname;
      if (!client->registered) {
        AddReply(pending, client_id,
                 ":ircserv 451 " + target + " :You have not registered\r\n");
      } else if (message.params.empty() || message.params[0].empty()) {
        AddReply(pending, client_id,
                 ":ircserv 461 " + target + " JOIN :Not enough parameters\r\n");
      } else {
        const std::string &names = message.params[0];
        // Record every change without allocating after membership is updated.
        joined.reserve(std::count(names.begin(), names.end(), ',') + 1);
        std::string::size_type start = 0;
        while (true) {
          const std::string::size_type end = names.find(',', start);
          JoinChannel(registration, client_id, names.substr(start, end - start),
                      pending, joined);
          if (end == std::string::npos)
            break;
          start = end + 1;
        }
      }
    }
  } catch (const std::bad_alloc &) {
    for (std::vector<ChannelMap::iterator>::reverse_iterator it = joined.rbegin();
         it != joined.rend(); ++it) {
      ChannelMap::iterator channel = *it;
      channel->second.members.erase(client_id);
      if (channel->second.members.empty())
        channels_.erase(channel);
    }
    return false;
  }

  replies.swap(pending);
  return true;
}

void Channels::JoinChannel(const Registration &registration, ClientId client_id,
                           const std::string &name,
                           std::vector<ChannelReply> &replies,
                           std::vector<ChannelMap::iterator> &joined) {
  if (!IsValidChannelName(name)) {
    const bool fits_parameter =
        !name.empty() && name.size() <= kMaxChannelLength && name[0] != ':' &&
        name.find_first_of(" \a\r\n") == std::string::npos &&
        name.find('\0') == std::string::npos;
    const std::string rejected_name = fits_parameter ? name : "*";
    AddReply(replies, client_id,
             ":ircserv 476 " + registration.FindClient(client_id)->nickname +
                 " " + rejected_name + " :Bad Channel Mask\r\n");
    return;
  }

  const std::string key = ChannelKey(name);
  ChannelMap::iterator channel = channels_.find(key);
  if (channel == channels_.end()) {
    Channel created;
    created.name = name;
    BuildJoinReplies(registration, client_id, created, kOperator, replies);
    created.members.insert(std::make_pair(client_id, kOperator));
    channel = channels_.insert(std::make_pair(key, created)).first;
  } else {
    Members &members = channel->second.members;
    if (members.find(client_id) != members.end())
      return;
    BuildJoinReplies(registration, client_id, channel->second, kMember, replies);
    members.insert(std::make_pair(client_id, kMember));
  }
  joined.push_back(channel);
}

void Channels::BuildJoinReplies(const Registration &registration,
                                ClientId client_id, const Channel &channel,
                                Role role, std::vector<ChannelReply> &replies) {
  const ClientState &client = *registration.FindClient(client_id);
  const std::string suffix = "@ircserv JOIN " + channel.name + "\r\n";
  const bool valid_username =
      client.username.size() <=
          kMaxLineLength - client.nickname.size() - suffix.size() - 2 &&
      client.username.find_first_of(" @\r\n") == std::string::npos &&
      client.username.find('\0') == std::string::npos;
  const std::string &username =
      valid_username ? client.username : client.nickname;
  const std::string join = ":" + client.nickname + "!" + username + suffix;
  AddReply(replies, client_id, join);

  const std::string header =
      ":ircserv 353 " + client.nickname + " = " + channel.name + " :";
  std::string line = header;
  for (Members::const_iterator it = channel.members.begin();
       it != channel.members.end(); ++it) {
    const ClientState *member = registration.FindClient(it->first);
    if (member == NULL || !member->registered)
      continue;
    AddReply(replies, it->first, join);
    AddName(replies, client_id, header, line,
            it->second == kOperator ? "@" + member->nickname : member->nickname);
  }
  AddName(replies, client_id, header, line,
          role == kOperator ? "@" + client.nickname : client.nickname);
  AddReply(replies, client_id, line + "\r\n");
  AddReply(replies, client_id,
           ":ircserv 366 " + client.nickname + " " + channel.name +
               " :End of NAMES list\r\n");
}
