#ifndef FT_IRC_REGISTRATION_HPP
#define FT_IRC_REGISTRATION_HPP

#include "ClientState.hpp"
#include "IrcMessage.hpp"
#include "Transport.hpp"

#include <string>
#include <map>
#include <vector>

class Registration
{
    public:
        explicit Registration(const std::string &server_password);

        bool AddClient(ClientId client_id);
        void RemoveClient(ClientId client_id);

        const ClientState *FindClient(ClientId client_id) const;

        bool HandleMessage(
            ClientId client_id,
            const IrcMessage &message,
            std::vector<std::string> &replies);
        
    private:
        std::string server_password_;
        std::map<ClientId, ClientState> clients_ ;

        static bool IsValidNickname(const std::string &nickname);

        void HandlePass(
            ClientState &client,
            const IrcMessage &message,
            std::vector<std::string> &replies);

        void HandleNick(
            ClientId client_id,
            ClientState &client,
            const IrcMessage &message,
            std::vector<std::string> &replies);

        void HandleUser(
            ClientState &client,
            const IrcMessage &message,
            std::vector<std::string> &replies);
        
        static void TryCompleteRegistration(
            ClientState &candidate,
            std::vector<std::string> &replies);
};

#endif
