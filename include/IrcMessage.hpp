#ifndef IRCMESSAGE_HPP
#define IRCMESSAGE_HPP

#include <string>
#include <vector>

struct IrcMessage{
    std::string command;
    std::vector<std::string> params;
};

bool parseMessage(const std::string &line, IrcMessage &out);

#endif
