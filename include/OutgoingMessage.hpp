#ifndef OUTGOINGMESSAGE_HPP
#define OUTGOINGMESSAGE_HPP

#include "ClientId.hpp"

#include <string>

struct OutgoingMessage {
  ClientId client_id;
  std::string line;
};

#endif
