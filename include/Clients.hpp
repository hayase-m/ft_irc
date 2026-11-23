#ifndef CLIENTS_HPP
#define CLIENTS_HPP

#include "ClientId.hpp"
#include "ClientState.hpp"

#include <map>
#include <string>

class Clients {
public:
  Clients();

  bool AddClient(ClientId client_id);

  void RemoveClient(ClientId client_id);

  const ClientState *FindClient(ClientId client_id) const;

  ClientState *FindClient(ClientId client_id);

  bool FindClientIdByNickname(const std::string &nickname,
                              ClientId &client_id) const;

private:
  std::map<ClientId, ClientState> clients_;

  Clients(const Clients &other);
  Clients &operator=(const Clients &other);
};

#endif
