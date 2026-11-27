#include "Channel.hpp"

#include <utility>

Channel::Channel(const std::string &name) : name_(name) {}

const std::string &Channel::GetName() const { return name_; }

const Channel::Members &Channel::GetMembers() const { return members_; }

bool Channel::HasMember(ClientId client_id) const {
  return members_.find(client_id) != members_.end();
}

bool Channel::AddMember(ClientId client_id, Role role) {
  return members_.insert(std::make_pair(client_id, role)).second;
}

void Channel::RemoveMember(ClientId client_id) { members_.erase(client_id); }

bool Channel::IsEmpty() const { return members_.empty(); }
