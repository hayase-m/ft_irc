#ifndef FT_IRC_CLIENT_STATE_HPP
#define FT_IRC_CLIENT_STATE_HPP

#include <string>

struct ClientState {
  ClientState()
      : authenticated(false), registered(false),
        has_connection_password(false) {}
  bool authenticated;
  std::string nickname;
  std::string username;
  std::string realname;
  bool registered;
  bool has_connection_password;
  std::string connection_password;
};

#endif
