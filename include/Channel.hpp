#ifndef CHANNEL_HPP
#define CHANNEL_HPP

#include "Transport.hpp"

#include <map>
#include <string>

class Channel {
public:
  enum Role { kMember, kOperator };
  typedef std::map<ClientId, Role> Members;

  explicit Channel(const std::string &name);

  const std::string &GetName() const;
  const Members &GetMembers() const;
  bool HasMember(ClientId client_id) const;
  bool AddMember(ClientId client_id, Role role);
  void RemoveMember(ClientId client_id);
  bool IsEmpty() const;

private:
  std::string name_;
  Members members_;
};

#endif
