#ifndef OUTGOINGMESSAGE_HPP
#define OUTGOINGMESSAGE_HPP

#include "ClientId.hpp"

#include <string>

struct OutgoingMessage {
  OutgoingMessage(ClientId client_id, const std::string &line)
      : client_id(client_id), line(line) {}
  ClientId client_id;
  std::string line;
};

#endif
