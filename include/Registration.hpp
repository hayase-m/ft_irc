#ifndef FT_IRC_REGISTRATION_HPP
#define FT_IRC_REGISTRATION_HPP

#include "ClientId.hpp"
#include "ClientState.hpp"
#include "Clients.hpp"
#include "IrcMessage.hpp"

#include <string>
#include <vector>

class Registration {
public:
  Registration(const std::string &server_password, Clients &clients);

  bool HandleMessage(ClientId client_id, const IrcMessage &message,
                     std::vector<std::string> &replies);

private:
  std::string server_password_;
  Clients &clients_;

  void HandlePass(ClientState &client, const IrcMessage &message,
                  std::vector<std::string> &replies);

  void HandleNick(ClientId client_id, ClientState &client,
                  const IrcMessage &message, std::vector<std::string> &replies);

  void HandleUser(ClientState &client, const IrcMessage &message,
                  std::vector<std::string> &replies);

  Registration(const Registration &other);
  Registration &operator=(const Registration &other);
};

#endif
