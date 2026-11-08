#ifndef FT_IRC_CONNECTION_HPP
#define FT_IRC_CONNECTION_HPP

#include <string>
#include <vector>

class Connection {
public:
  explicit Connection(int fd);
  ~Connection();

  int GetSocketFd() const;
  bool GetWantsToSend() const;
  void SetReadReady(bool read_ready);

  bool Receive(std::vector<std::string> &data);

private:
  int fd_;
  bool wants_to_send_;
  bool read_ready_;
  std::string read_buffer_;

  void ExtractLines(std::vector<std::string> &data);

  Connection(const Connection &other);
  Connection &operator=(const Connection &other);
};

#endif
