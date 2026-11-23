#ifndef CHANNELS_HPP
#define CHANNELS_HPP

#include "ClientId.hpp"
#include "OutgoingMessage.hpp"

#include <map>
#include <string>
#include <vector>

struct IrcMessage;
class Clients;

class Channels {
    public:
        explicit Channels(const Clients &clients);

        bool HandleJoin(ClientId client_id, const IrcMessage &message,
                        std::vector<OutgoingMessage> &replies);

    private:
        enum Role { kMember, kOperator };
        typedef std::map<ClientId, Role> Members;

        struct Channel {
            std::string name;
            Members members;
        };

        typedef std::map<std::string, Channel> ChannelMap;

        const Clients &clients_;
        ChannelMap channels_;

        void JoinChannel(ClientId client_id, const std::string &name,
                        std::vector<OutgoingMessage> &replies,
                        std::vector<ChannelMap::iterator> &joined);

        void BuildJoinReplies(ClientId client_id, const Channel &channel,
                              Role role,
                              std::vector<OutgoingMessage> &replies) const;

        Channels(const Channels &other);
        Channels &operator=(const Channels &other);
};

#endif
