#ifndef FT_IRC_TRANSPORT_HPP
#define FT_IRC_TRANSPORT_HPP

#include <map>
#include <string>
#include <vector>

class Connection;

typedef unsigned int ClientId;

struct TransportEvent {
  enum Type { kConnected, kLineReceived, kDisconnected };

  Type type;
  ClientId client_id;
  std::string line;
};

class Transport {
public:
  explicit Transport(unsigned short port);
  ~Transport();

  std::vector<TransportEvent> WaitForEvents();
  void Disconnect(ClientId client_id);

private:
  int listen_fd_;
  std::map<ClientId, Connection *> clients_;
  ClientId next_client_id_;
  std::vector<ClientId> disconnected_client_ids_;

  void AcceptClient(std::vector<TransportEvent> &events);

  Transport(const Transport &other);
  Transport &operator=(const Transport &other);
};

#endif
