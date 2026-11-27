#ifndef CHANNELS_HPP
#define CHANNELS_HPP

#include "Channel.hpp"
#include "Transport.hpp"

#include <map>
#include <string>
#include <vector>

struct IrcMessage;

struct ChannelReply {
  ClientId client_id;
  std::string message;
};

class Registration;

class Channels {
public:
  bool HandleJoin(const Registration &registration, ClientId client_id,
                  const IrcMessage &message,
                  std::vector<ChannelReply> &replies);

private:
  typedef std::map<std::string, Channel> ChannelMap;
  ChannelMap channels_;

  void JoinChannel(const Registration &registration, ClientId client_id,
                   const std::string &name, std::vector<ChannelReply> &replies,
                   std::vector<ChannelMap::iterator> &joined);

  static void BuildJoinReplies(const Registration &registration,
                               ClientId client_id, const Channel &channel,
                               Channel::Role role,
                               std::vector<ChannelReply> &replies);
};

#endif
