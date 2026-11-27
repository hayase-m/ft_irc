#ifndef CHANNELS_HPP
#define CHANNELS_HPP

#include "Transport.hpp"

#include <map>
#include <string>
#include <vector>

struct IrcMessage;

struct ChannelReply {
  ClientId client_id;
  std::string message; // The send queue appends CRLF.
};

class Registration;

class Channel {
public:
  enum Role { kMember, kOperator };
  typedef std::map<ClientId, Role> Members;

  explicit Channel(const std::string &name);

  const std::string &GetName() const;
  const Members &GetMembers() const;
  bool HasMember(ClientId client_id) const;
  bool AddMember(ClientId client_id, Role role);
  void RemoveMember(ClientId client_id);
  bool IsEmpty() const;

private:
  std::string name_;
  Members members_;
};

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
