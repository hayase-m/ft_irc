#include "Connection.hpp"

#include <new>
#include <stdexcept>

#include <sys/socket.h>
#include <unistd.h>

Connection::Connection(int fd)
    : fd_(fd), wants_to_send_(false), read_ready_(false) {}

Connection::~Connection() { close(fd_); }

int Connection::GetSocketFd() const { return fd_; }

bool Connection::GetWantsToSend() const { return wants_to_send_; }

void Connection::SetReadReady(bool read_ready) { read_ready_ = read_ready; }

bool Connection::Receive(std::vector<std::string> &data) {
  data.clear();
  if (!read_ready_)
    throw std::logic_error("Receive requires a readable event");
  read_ready_ = false;
  char buffer[4096];
  const ssize_t received = recv(fd_, buffer, sizeof(buffer), 0);
  if (received <= 0)
    return false;
  try {
    read_buffer_.append(buffer, static_cast<std::string::size_type>(received));
    ExtractLines(data);
  } catch (const std::bad_alloc &) {
    data.clear();
    return false;
  }
  return true;
}

void Connection::ExtractLines(std::vector<std::string> &data) {
  while (true) {
    const std::string::size_type pos = read_buffer_.find("\r\n");
    if (pos == std::string::npos)
      break;
    data.push_back(read_buffer_.substr(0, pos));
    read_buffer_.erase(0, pos + 2);
  }
}
