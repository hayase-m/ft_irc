#ifndef FT_IRC_CLIENT_STATE_HPP
#define FT_IRC_CLIENT_STATE_HPP

#include <string>

struct ClientState{
        
    ClientState(): 
        authenticated(false), registered(false){}
    bool authenticated;
    std::string nickname;
    std::string username;
    std::string realname;
    bool registered;
};

#endif
