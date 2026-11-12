#include "Connection.hpp"

#include <new>

#include <sys/socket.h>
#include <unistd.h>

Connection::Connection(int fd) : fd_(fd) {}

Connection::~Connection() { close(fd_); }

int Connection::GetSocketFd() const { return fd_; }

bool Connection::ReceiveLines(std::vector<std::string> &lines) {
  char buffer[4096];
  const ssize_t received = recv(fd_, buffer, sizeof(buffer), 0);
  if (received <= 0)
    return false;
  try {
    read_buffer_.append(buffer, static_cast<std::string::size_type>(received));
    while (true) {
      const std::string::size_type pos = read_buffer_.find("\r\n");
      if (pos == std::string::npos)
        break;
      lines.push_back(read_buffer_.substr(0, pos));
      read_buffer_.erase(0, pos + 2);
    }
  } catch (const std::bad_alloc &) {
    return false;
  }
  return true;
}
