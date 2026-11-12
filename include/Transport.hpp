#ifndef TRANSPORT_HPP
#define TRANSPORT_HPP

#include <map>
#include <string>
#include <vector>

typedef unsigned int ClientId;

struct TransportEvent {
  enum Type { kConnected, kReadable, kWritable, kDisconnected };

  Type type;
  ClientId client_id;
};

class Transport {
public:
  explicit Transport(unsigned short port);
  ~Transport();

  std::vector<TransportEvent> WaitForEvents();
  bool Receive(ClientId client_id, std::vector<std::string> &data);
  void Disconnect(ClientId client_id);

private:
  struct Client {
    int fd;
    bool wants_to_send;
    bool read_ready;
    std::string read_buffer;
  };

  int listen_fd_;
  std::map<ClientId, Client> clients_;
  ClientId next_client_id_;
  std::vector<TransportEvent> disconnected_events_;

  void AcceptClient(std::vector<TransportEvent> &events);

  Transport(const Transport &other);
  Transport &operator=(const Transport &other);
};

#endif
