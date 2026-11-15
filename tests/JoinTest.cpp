#include "Channels.hpp"
#include "IrcMessage.hpp"
#include "Registration.hpp"

#include <cassert>
#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <new>
#include <sstream>
#include <string>
#include <vector>

namespace {

int allocations_before_failure = -1;

void Register(Registration &registration, ClientId client_id,
              const std::string &nickname) {
  assert(registration.AddClient(client_id));

  const std::string commands[] = {"PASS secret", "NICK " + nickname,
                                   "USER guest 0 * :Guest"};
  IrcMessage message;
  std::vector<std::string> registration_replies;
  for (std::size_t i = 0; i < sizeof(commands) / sizeof(commands[0]); ++i) {
    assert(parseMessage(commands[i], message));
    assert(registration.HandleMessage(client_id, message, registration_replies));
  }
  const ClientState *client = registration.FindClient(client_id);
  assert(client != NULL && client->registered);
}

std::vector<ChannelReply> SendJoin(Channels &channels,
                                   const Registration &registration,
                                   ClientId client_id, const std::string &line) {
  IrcMessage message;
  std::vector<ChannelReply> replies;
  assert(parseMessage(line, message));
  assert(channels.HandleJoin(registration, client_id, message, replies));
  return replies;
}

void ExpectReply(const ChannelReply &reply, ClientId client_id,
                 const std::string &message) {
  assert(reply.client_id == client_id);
  assert(reply.message == message);
}

void TestFirstJoinMakesCreatorOperator() {
  const ClientId client_id = 1;
  Registration registration("secret");
  Register(registration, client_id, "Taro");

  Channels channels;
  const std::vector<ChannelReply> replies =
      SendJoin(channels, registration, client_id, "JOIN #room");

  assert(replies.size() == 3);
  ExpectReply(replies[0], client_id, ":Taro!guest@ircserv JOIN #room\r\n");
  ExpectReply(replies[1], client_id, ":ircserv 353 Taro = #room :@Taro\r\n");
  ExpectReply(replies[2], client_id,
              ":ircserv 366 Taro #room :End of NAMES list\r\n");
}

void TestMembersAndOperatorsAreScopedToEachChannel() {
  Registration registration("secret");
  Register(registration, 1, "Taro");
  Register(registration, 2, "Hanako");
  Register(registration, 3, "Jiro");

  Channels channels;
  SendJoin(channels, registration, 1, "JOIN #room");
  SendJoin(channels, registration, 3, "JOIN #other");

  const std::vector<ChannelReply> replies =
      SendJoin(channels, registration, 2, "JOIN #ROOM,#second");
  assert(replies.size() == 7);
  ExpectReply(replies[0], 2, ":Hanako!guest@ircserv JOIN #room\r\n");
  ExpectReply(replies[1], 1, ":Hanako!guest@ircserv JOIN #room\r\n");
  ExpectReply(replies[2], 2, ":ircserv 353 Hanako = #room :@Taro Hanako\r\n");
  ExpectReply(replies[3], 2, ":ircserv 366 Hanako #room :End of NAMES list\r\n");
  ExpectReply(replies[4], 2, ":Hanako!guest@ircserv JOIN #second\r\n");
  ExpectReply(replies[5], 2, ":ircserv 353 Hanako = #second :@Hanako\r\n");
  ExpectReply(replies[6], 2, ":ircserv 366 Hanako #second :End of NAMES list\r\n");
  assert(SendJoin(channels, registration, 2, "JOIN #room").empty());

  const std::vector<ChannelReply> next =
      SendJoin(channels, registration, 3, "JOIN #room");
  assert(next.size() == 5);
  ExpectReply(next[3], 3, ":ircserv 353 Jiro = #room :@Taro Hanako Jiro\r\n");
}

void TestRejectedRequestsAndChannelLength() {
  Registration registration("secret");
  Register(registration, 1, "Taro");
  assert(registration.AddClient(2));
  Channels channels;

  std::vector<ChannelReply> replies =
      SendJoin(channels, registration, 2, "JOIN #room");
  assert(replies.size() == 1);
  ExpectReply(replies[0], 2, ":ircserv 451 * :You have not registered\r\n");

  replies = SendJoin(channels, registration, 1, "JOIN");
  assert(replies.size() == 1);
  ExpectReply(replies[0], 1, ":ircserv 461 Taro JOIN :Not enough parameters\r\n");

  replies = SendJoin(channels, registration, 1, "JOIN :#two words");
  assert(replies.size() == 1);
  ExpectReply(replies[0], 1, ":ircserv 476 Taro * :Bad Channel Mask\r\n");

  const std::string max_name = "#" + std::string(49, 'x');
  replies = SendJoin(channels, registration, 1, "JOIN " + max_name + "x");
  assert(replies.size() == 1);
  ExpectReply(replies[0], 1, ":ircserv 476 Taro * :Bad Channel Mask\r\n");

  replies = SendJoin(channels, registration, 1, "JOIN " + max_name);
  assert(replies.size() == 3);
  ExpectReply(replies[1], 1, ":ircserv 353 Taro = " + max_name + " :@Taro\r\n");

  replies = SendJoin(channels, registration, 1, "JOIN #room");
  assert(replies.size() == 3);
  ExpectReply(replies[1], 1, ":ircserv 353 Taro = #room :@Taro\r\n");
}

void TestLongNamesListIsSplit() {
  Registration registration("secret");
  Channels channels;
  const ClientId last_client = 64;
  for (ClientId id = 1; id <= last_client; ++id) {
    std::ostringstream nickname;
    nickname << "Member" << id;
    Register(registration, id, nickname.str());
    if (id < last_client)
      SendJoin(channels, registration, id, "JOIN #room");
  }

  const std::vector<ChannelReply> replies =
      SendJoin(channels, registration, last_client, "JOIN #room");
  assert(replies.size() == last_client + 3);
  const std::string header = ":ircserv 353 Member64 = #room :";
  const std::string notice = ":Member64!guest@ircserv JOIN #room\r\n";
  ExpectReply(replies.front(), last_client, notice);
  ClientId next_member = 1;
  std::string names;
  for (std::size_t i = 1; i + 1 < replies.size(); ++i) {
    if (replies[i].client_id != last_client) {
      ExpectReply(replies[i], next_member, notice);
      ++next_member;
      continue;
    }
    const std::string &line = replies[i].message;
    assert(line.size() <= 512);
    assert(line.find(header) == 0);
    assert(line.compare(line.size() - 2, 2, "\r\n") == 0);
    names += line.substr(header.size(), line.size() - header.size() - 2) + " ";
  }
  assert(next_member == last_client);
  std::ostringstream expected;
  expected << "@Member1 ";
  for (ClientId id = 2; id <= last_client; ++id)
    expected << "Member" << id << " ";
  assert(names == expected.str());
  ExpectReply(replies.back(), last_client,
              ":ircserv 366 Member64 #room :End of NAMES list\r\n");
}

void TestAllocationFailureRollsBackMembershipAndReplies() {
  Registration registration("secret");
  Register(registration, 1, "Taro");
  Register(registration, 2, "Hanako");
  Channels channels;
  SendJoin(channels, registration, 1, "JOIN #room");

  IrcMessage message;
  assert(parseMessage("JOIN #room,#second", message));
  const ChannelReply previous = {1, "previous reply"};
  std::vector<ChannelReply> replies(1, previous);

  for (int allocation = 0; allocation < 256; ++allocation) {
    allocations_before_failure = allocation;
    const bool succeeded = channels.HandleJoin(registration, 2, message, replies);
    allocations_before_failure = -1;
    if (succeeded) {
      assert(replies.size() == 7);
      ExpectReply(replies[2], 2, ":ircserv 353 Hanako = #room :@Taro Hanako\r\n");
      ExpectReply(replies[5], 2, ":ircserv 353 Hanako = #second :@Hanako\r\n");
      return;
    }
    assert(replies.size() == 1);
    ExpectReply(replies[0], 1, "previous reply");
  }
  assert(false);
}

} // namespace

void *operator new(std::size_t size) throw(std::bad_alloc) {
  if (allocations_before_failure == 0)
    throw std::bad_alloc();
  if (allocations_before_failure > 0)
    --allocations_before_failure;
  void *memory = std::malloc(size == 0 ? 1 : size);
  if (memory == NULL)
    throw std::bad_alloc();
  return memory;
}

void operator delete(void *memory) throw() { std::free(memory); }

int main() {
  TestFirstJoinMakesCreatorOperator();
  TestMembersAndOperatorsAreScopedToEachChannel();
  TestRejectedRequestsAndChannelLength();
  TestLongNamesListIsSplit();
  TestAllocationFailureRollsBackMembershipAndReplies();
  std::cout << "JOIN tests passed" << std::endl;
}
