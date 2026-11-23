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

class Channels {
    public:
        bool HandleJoin(const Registration &registration, ClientId client_id,
                        const IrcMessage &message,
                        std::vector<ChannelReply> &replies);

    private:
        enum Role { kMember, kOperator };
        typedef std::map<ClientId, Role> Members;

        struct Channel {
            std::string name;
            Members members;
        };

        typedef std::map<std::string, Channel> ChannelMap;
        ChannelMap channels_;

        void JoinChannel(const Registration &registration, ClientId client_id,
                        const std::string &name, std::vector<ChannelReply> &replies,
                        std::vector<ChannelMap::iterator> &joined);

        static void BuildJoinReplies(const Registration &registration,
                                    ClientId client_id, const Channel &channel,
                                    Role role, std::vector<ChannelReply> &replies);
};

#endif
