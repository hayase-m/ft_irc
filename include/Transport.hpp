#ifndef TRANSPORT_HPP
#define TRANSPORT_HPP

#include <map>
#include <poll.h>
#include <string>
#include <vector>

class Connection;

typedef unsigned int ClientId;

struct TransportEvent {
  enum Type { kConnected, kLineReceived };

  Type type;
  ClientId client_id;
  std::string line;
};

class Transport {
public:
  explicit Transport(unsigned short port);
  ~Transport();

  std::vector<TransportEvent> WaitForEvents();
  bool PopDisconnectedClientId(ClientId &client_id);
  void Disconnect(ClientId client_id);

private:
  int listen_fd_;
  std::map<ClientId, Connection *> clients_;
  ClientId next_client_id_;
  std::vector<ClientId> disconnected_client_ids_;
  std::vector<pollfd> poll_fds_;

  void BuildPollFds();
  void HandleConnectionEvents(std::vector<TransportEvent> &events);
  void AcceptClient(std::vector<TransportEvent> &events);

  Transport(const Transport &other);
  Transport &operator=(const Transport &other);
};

#endif
