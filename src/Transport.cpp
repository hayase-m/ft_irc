#include "Transport.hpp"
#include "Connection.hpp"

#include <cerrno>
#include <cstring>
#include <new>
#include <stdexcept>
#include <string>

#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

namespace {
const ClientId kExhaustedClientId = 0;

void ThrowSocketError(const std::string &operation) {
  throw std::runtime_error(operation + ": " + std::strerror(errno));
}

void AddEvent(std::vector<TransportEvent> &events, TransportEvent::Type type,
              ClientId client_id) {
  TransportEvent event;
  event.type = type;
  event.client_id = client_id;
  events.push_back(event);
}

void AppendDisconnectedEvents(std::vector<ClientId> &client_ids,
                              std::vector<TransportEvent> &events) {
  try {
    events.reserve(events.size() + client_ids.size());
  } catch (const std::bad_alloc &) {
    return;
  }
  for (std::size_t i = 0; i < client_ids.size(); ++i)
    AddEvent(events, TransportEvent::kDisconnected, client_ids[i]);
  client_ids.clear();
}

bool ReceiveClientLines(Connection &connection, ClientId client_id,
                        std::vector<TransportEvent> &events) {
  std::vector<std::string> lines;
  if (!connection.ReceiveLines(lines))
    return false;
  try {
    events.reserve(events.size() + lines.size());
  } catch (const std::bad_alloc &) {
    return false;
  }
  for (std::size_t i = 0; i < lines.size(); ++i) {
    AddEvent(events, TransportEvent::kLineReceived, client_id);
    events.back().line.swap(lines[i]);
  }
  return true;
}
} // namespace

Transport::Transport(unsigned short port) : listen_fd_(-1), next_client_id_(1) {
  listen_fd_ = socket(AF_INET, SOCK_STREAM, 0);
  if (listen_fd_ == -1)
    ThrowSocketError("socket");
  try {
    if (fcntl(listen_fd_, F_SETFL, O_NONBLOCK) == -1)
      ThrowSocketError("fcntl");
    const int yes = 1;
    if (setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) ==
        -1)
      ThrowSocketError("setsockopt");
    sockaddr_in address = {};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(port);
    if (bind(listen_fd_, reinterpret_cast<sockaddr *>(&address),
             sizeof(address)) == -1)
      ThrowSocketError("bind");
    if (listen(listen_fd_, SOMAXCONN) == -1)
      ThrowSocketError("listen");
    poll_fds_.reserve(1);
  } catch (...) {
    close(listen_fd_);
    throw;
  }
}

Transport::~Transport() {
  for (std::map<ClientId, Connection *>::const_iterator it = clients_.begin();
       it != clients_.end(); ++it)
    delete it->second;
  close(listen_fd_);
}

std::vector<TransportEvent> Transport::WaitForEvents() {
  std::vector<TransportEvent> events;
  if (!disconnected_client_ids_.empty()) {
    AppendDisconnectedEvents(disconnected_client_ids_, events);
    return events;
  }
  BuildPollFds();
  if (poll(&poll_fds_[0], poll_fds_.size(), -1) == -1) {
    if (errno == EINTR)
      return events;
    ThrowSocketError("poll");
  }
  if (poll_fds_[0].revents & (POLLERR | POLLHUP | POLLNVAL))
    throw std::runtime_error("listening socket is unavailable");
  HandleConnectionEvents(events);
  if (poll_fds_[0].revents & POLLIN)
    AcceptClient(events);
  AppendDisconnectedEvents(disconnected_client_ids_, events);
  return events;
}

void Transport::BuildPollFds() {
  poll_fds_.clear();
  pollfd listener = {listen_fd_, POLLIN, 0};
  poll_fds_.push_back(listener);
  for (std::map<ClientId, Connection *>::iterator it = clients_.begin();
       it != clients_.end(); ++it) {
    pollfd client_poll_fd = {it->second->GetSocketFd(), POLLIN, 0};
    poll_fds_.push_back(client_poll_fd);
  }
}

void Transport::HandleConnectionEvents(std::vector<TransportEvent> &events) {
  std::size_t index = 1;
  std::map<ClientId, Connection *>::iterator it = clients_.begin();
  while (it != clients_.end()) {
    std::map<ClientId, Connection *>::iterator current = it++;
    const ClientId client_id = current->first;
    Connection &connection = *current->second;
    const short ready = poll_fds_[index++].revents;
    if ((ready & (POLLERR | POLLNVAL)) ||
        ((ready & POLLHUP) && !(ready & POLLIN))) {
      Disconnect(client_id);
      continue;
    }
    if ((ready & POLLIN) && !ReceiveClientLines(connection, client_id, events))
      Disconnect(client_id);
  }
}

void Transport::AcceptClient(std::vector<TransportEvent> &events) {
  const int client_fd = accept(listen_fd_, NULL, NULL);
  if (client_fd == -1)
    return;
  if (next_client_id_ == kExhaustedClientId) {
    close(client_fd);
    return;
  }
  if (fcntl(client_fd, F_SETFL, O_NONBLOCK) == -1) {
    close(client_fd);
    return;
  }
  Connection *connection = NULL;
  try {
    disconnected_client_ids_.reserve(disconnected_client_ids_.size() +
                                     clients_.size() + 1);
    events.reserve(events.size() + 1);
    poll_fds_.reserve(poll_fds_.size() + 1);
    connection = new Connection(client_fd);
    clients_.insert(std::make_pair(next_client_id_, connection));
  } catch (const std::bad_alloc &) {
    if (connection != NULL)
      delete connection;
    else
      close(client_fd);
    return;
  }
  AddEvent(events, TransportEvent::kConnected, next_client_id_);
  ++next_client_id_;
}

void Transport::Disconnect(ClientId client_id) {
  std::map<ClientId, Connection *>::iterator it = clients_.find(client_id);
  if (it == clients_.end())
    return;
  disconnected_client_ids_.push_back(client_id);
  delete it->second;
  clients_.erase(it);
}
