#include "IrcMessage.hpp"

bool parseMessage(const std::string &line, IrcMessage &out)
{
  out.command.clear();
  out.params.clear();

  if(line.find_first_of("\r\n") != std::string::npos
      || line.find('\0') != std::string::npos)
      return false;
  
  std::string::size_type pos = line.find_first_not_of(' ');
  if(pos == std::string::npos)
    return false;

  IrcMessage message;

  const std::string::size_type command_start = pos;
  while(pos < line.size() && line[pos] != ' ')
    ++pos;

  message.command = line.substr(command_start, pos - command_start);

  const bool alphabetic = message.command.find_first_not_of(
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz")
      == std::string::npos;
  const bool numeric = message.command.size() == 3
      && message.command.find_first_not_of("0123456789")
      == std::string::npos;
  
  if(!alphabetic && !numeric)
    return false;

  for(std::string::size_type i = 0; i < message.command.size(); ++i)
  {
    if(message.command[i] >= 'a' && message.command[i] <= 'z')
      message.command[i] = message.command[i] - 'a' + 'A';
  }

  while(pos < line.size())
  {
    pos = line.find_first_not_of(' ', pos);
    if (pos == std::string::npos)
      break;
    if(line[pos] == ':')
    {
      message.params.push_back(line.substr(pos + 1));
      break;
    }

    const std::string::size_type param_start = pos;
    while(pos < line.size() && line[pos] != ' ')
      ++pos;

    message.params.push_back(line.substr(param_start, pos - param_start));
  }

  out = message;
  return true;
}
