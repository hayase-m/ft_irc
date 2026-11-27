#include "Connection.hpp"

#include <new>

#include <sys/socket.h>
#include <unistd.h>

namespace {
const std::string::size_type kCrlfLength = 2;
const std::string::size_type kMaxLineLength = 512;
const std::string::size_type kMaxWriteBufferSize = 200 * 1024;
} // namespace

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
      const std::string::size_type line_end = read_buffer_.find("\r\n");
      if (line_end == std::string::npos)
        break;
      const std::string line = read_buffer_.substr(0, line_end);
      if (line.size() > 510)
        return false;
      lines.push_back(line);
      read_buffer_.erase(0, line_end + kCrlfLength);
    }
    if (read_buffer_.empty())
      return true;
    if (read_buffer_.size() >= kMaxLineLength)
      return false;
    if (read_buffer_[read_buffer_.size() - 1] != '\r' &&
        (read_buffer_.size() >= kMaxLineLength - 1))
      return false;
  } catch (const std::bad_alloc &) {
    return false;
  }
  return true;
}

bool Connection::QueueLineToSend(const std::string &line) {
  if (write_buffer_.size() + line.size() + kCrlfLength > kMaxWriteBufferSize)
    return false;
  try {
    write_buffer_.append(line + "\r\n");
  } catch (const std::bad_alloc &) {
    return false;
  }
  return true;
}

bool Connection::HasDataToSend() const { return !write_buffer_.empty(); }

bool Connection::SendQueuedData() {
  const ssize_t sent =
      send(fd_, write_buffer_.c_str(), write_buffer_.size(), 0);
  if (sent == -1)
    return false;
  write_buffer_.erase(0, static_cast<std::string::size_type>(sent));
  return true;
}
