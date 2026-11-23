#include "Clients.hpp"

#include <cstddef>
#include <new>
#include <utility>

namespace {

std::string NicknameKey(const std::string &nickname) {
  const std::string upper = "ABCDEFGHIJKLMNOPQRSTUVWXYZ[]\\^";
  const std::string lower = "abcdefghijklmnopqrstuvwxyz{}|~";
  std::string key = nickname;

  for (std::string::size_type i = 0; i < key.size(); ++i) {
    const std::string::size_type index = upper.find(key[i]);

    if (index != std::string::npos)
      key[i] = lower[index];
  }
  return key;
}

} // namespace

Clients::Clients() {}

bool Clients::AddClient(ClientId client_id) {
  try {
    return clients_.insert(std::make_pair(client_id, ClientState())).second;
  } catch (const std::bad_alloc &) {
    return false;
  }
}

void Clients::RemoveClient(ClientId client_id) { clients_.erase(client_id); }

const ClientState *Clients::FindClient(ClientId client_id) const {
  std::map<ClientId, ClientState>::const_iterator client =
      clients_.find(client_id);

  if (client == clients_.end())
    return NULL;

  return &client->second;
}

ClientState *Clients::FindClient(ClientId client_id) {
  std::map<ClientId, ClientState>::iterator client = clients_.find(client_id);

  if (client == clients_.end())
    return NULL;

  return &client->second;
}

bool Clients::FindClientIdByNickname(const std::string &nickname,
                                     ClientId &client_id) const {
  const std::string nickname_key = NicknameKey(nickname);

  for (std::map<ClientId, ClientState>::const_iterator client =
           clients_.begin();
       client != clients_.end(); ++client) {
    if (NicknameKey(client->second.nickname) == nickname_key) {
      client_id = client->first;
      return true;
    }
  }
  return false;
}
