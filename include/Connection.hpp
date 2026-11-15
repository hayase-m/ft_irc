#ifndef FT_IRC_CONNECTION_HPP
#define FT_IRC_CONNECTION_HPP

#include <string>
#include <vector>

class Connection {
public:
  explicit Connection(int fd);
  ~Connection();

  int GetSocketFd() const;
  bool ReceiveLines(std::vector<std::string> &lines);
  bool QueueLineToSend(const std::string &line);
  bool HasDataToSend() const;
  bool SendQueuedData();

private:
  int fd_;
  std::string read_buffer_;
  std::string write_buffer_;

  Connection(const Connection &other);
  Connection &operator=(const Connection &other);
};

#endif
