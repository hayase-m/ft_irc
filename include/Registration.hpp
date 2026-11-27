#ifndef FT_IRC_REGISTRATION_HPP
#define FT_IRC_REGISTRATION_HPP

#include "ClientId.hpp"
#include "ClientState.hpp"
#include "Clients.hpp"
#include "ConnectionAction.hpp"
#include "IrcMessage.hpp"
#include "OutgoingMessage.hpp"

#include <string>
#include <vector>

class Registration {
public:
  Registration(const std::string &server_password, Clients &clients);

  ConnectionAction HandleMessage(ClientId client_id, const IrcMessage &message,
                                 std::vector<OutgoingMessage> &replies);

private:
  std::string server_password_;
  Clients &clients_;

  void HandlePass(ClientId client_id, ClientState &client,
                  const IrcMessage &message,
                  std::vector<OutgoingMessage> &replies);

  void HandleNick(ClientId client_id, ClientState &client,
                  const IrcMessage &message,
                  std::vector<OutgoingMessage> &replies);

  void HandleUser(ClientId client_id, ClientState &client,
                  const IrcMessage &message,
                  std::vector<OutgoingMessage> &replies);

  ConnectionAction TryCompleteRegistration(
      ClientId client_id, ClientState &candidate,
      std::vector<OutgoingMessage> &replies);

  Registration(const Registration &other);
  Registration &operator=(const Registration &other);
};

#endif
