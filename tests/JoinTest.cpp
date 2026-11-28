#include "Channels.hpp"
#include "Clients.hpp"
#include "IrcMessage.hpp"
#include "OutgoingMessage.hpp"
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

void Register(Clients &clients, Registration &registration, ClientId client_id,
              const std::string &nickname) {
  assert(clients.AddClient(client_id));

  const std::string commands[] = {"PASS secret", "NICK " + nickname,
                                   "USER guest 0 * :Guest"};
  IrcMessage message;
  std::vector<OutgoingMessage> registration_replies;
  for (std::size_t i = 0; i < sizeof(commands) / sizeof(commands[0]); ++i) {
    assert(parseMessage(commands[i], message));
    assert(registration.HandleMessage(client_id, message, registration_replies));
  }
  const ClientState *client = clients.FindClient(client_id);
  assert(client != NULL && client->registered);
}

std::vector<OutgoingMessage> SendJoin(Channels &channels, ClientId client_id,
                                      const std::string &line) {
  IrcMessage message;
  std::vector<OutgoingMessage> replies;
  assert(parseMessage(line, message));
  assert(channels.HandleJoin(client_id, message, replies));
  return replies;
}

void ExpectReply(const OutgoingMessage &reply, ClientId client_id,
                 const std::string &message) {
  assert(reply.client_id == client_id);
  assert(reply.line == message);
}

void TestFirstJoinMakesCreatorOperator() {
  const ClientId client_id = 1;
  Clients clients;
  Registration registration("secret", clients);
  Register(clients, registration, client_id, "Taro");

  Channels channels(clients);
  const std::vector<OutgoingMessage> replies =
      SendJoin(channels, client_id, "JOIN #room");

  assert(replies.size() == 3);
  ExpectReply(replies[0], client_id, ":Taro!guest@ircserv JOIN #room");
  ExpectReply(replies[1], client_id, ":ircserv 353 Taro = #room :@Taro");
  ExpectReply(replies[2], client_id,
              ":ircserv 366 Taro #room :End of NAMES list");
}

void TestMembersAndOperatorsAreScopedToEachChannel() {
  Clients clients;
  Registration registration("secret", clients);
  Register(clients, registration, 1, "Taro");
  Register(clients, registration, 2, "Hanako");
  Register(clients, registration, 3, "Jiro");

  Channels channels(clients);
  SendJoin(channels, 1, "JOIN #room");
  SendJoin(channels, 3, "JOIN #other");

  const std::vector<OutgoingMessage> replies =
      SendJoin(channels, 2, "JOIN #ROOM,#second");
  assert(replies.size() == 7);
  ExpectReply(replies[0], 2, ":Hanako!guest@ircserv JOIN #room");
  ExpectReply(replies[1], 1, ":Hanako!guest@ircserv JOIN #room");
  ExpectReply(replies[2], 2, ":ircserv 353 Hanako = #room :@Taro Hanako");
  ExpectReply(replies[3], 2, ":ircserv 366 Hanako #room :End of NAMES list");
  ExpectReply(replies[4], 2, ":Hanako!guest@ircserv JOIN #second");
  ExpectReply(replies[5], 2, ":ircserv 353 Hanako = #second :@Hanako");
  ExpectReply(replies[6], 2, ":ircserv 366 Hanako #second :End of NAMES list");
  assert(SendJoin(channels, 2, "JOIN #room").empty());

  const std::vector<OutgoingMessage> next = SendJoin(channels, 3, "JOIN #room");
  assert(next.size() == 5);
  ExpectReply(next[3], 3, ":ircserv 353 Jiro = #room :@Taro Hanako Jiro");
}

void TestRejectedRequestsAndChannelLength() {
  Clients clients;
  Registration registration("secret", clients);
  Register(clients, registration, 1, "Taro");
  assert(clients.AddClient(2));
  Channels channels(clients);

  std::vector<OutgoingMessage> replies = SendJoin(channels, 2, "JOIN #room");
  assert(replies.size() == 1);
  ExpectReply(replies[0], 2, ":ircserv 451 * :You have not registered");

  replies = SendJoin(channels, 1, "JOIN");
  assert(replies.size() == 1);
  ExpectReply(replies[0], 1, ":ircserv 461 Taro JOIN :Not enough parameters");

  replies = SendJoin(channels, 1, "JOIN :#two words");
  assert(replies.size() == 1);
  ExpectReply(replies[0], 1, ":ircserv 476 Taro * :Bad Channel Mask");

  const std::string max_name = "#" + std::string(49, 'x');
  replies = SendJoin(channels, 1, "JOIN " + max_name + "x");
  assert(replies.size() == 1);
  ExpectReply(replies[0], 1, ":ircserv 476 Taro * :Bad Channel Mask");

  replies = SendJoin(channels, 1, "JOIN " + max_name);
  assert(replies.size() == 3);
  ExpectReply(replies[1], 1, ":ircserv 353 Taro = " + max_name + " :@Taro");

  replies = SendJoin(channels, 1, "JOIN #room");
  assert(replies.size() == 3);
  ExpectReply(replies[1], 1, ":ircserv 353 Taro = #room :@Taro");
}

void TestLongNamesListIsSplit() {
  Clients clients;
  Registration registration("secret", clients);
  Channels channels(clients);
  const ClientId last_client = 64;
  for (ClientId id = 1; id <= last_client; ++id) {
    std::ostringstream nickname;
    nickname << "Member" << id;
    Register(clients, registration, id, nickname.str());
    if (id < last_client)
      SendJoin(channels, id, "JOIN #room123");
  }

  const std::vector<OutgoingMessage> replies =
      SendJoin(channels, last_client, "JOIN #room123");
  assert(replies.size() == last_client + 3);
  const std::string header = ":ircserv 353 Member64 = #room123 :";
  const std::string notice = ":Member64!guest@ircserv JOIN #room123";
  ExpectReply(replies.front(), last_client, notice);
  ClientId next_member = 1;
  std::string names;
  for (std::size_t i = 1; i + 1 < replies.size(); ++i) {
    if (replies[i].client_id != last_client) {
      ExpectReply(replies[i], next_member, notice);
      ++next_member;
      continue;
    }
    const std::string &line = replies[i].line;
    assert(line.size() + 2 <= 512);
    assert(line.find(header) == 0);
    assert(line.find_first_of("\r\n") == std::string::npos);
    names += line.substr(header.size()) + " ";
  }
  assert(next_member == last_client);
  std::ostringstream expected;
  expected << "@Member1 ";
  for (ClientId id = 2; id <= last_client; ++id)
    expected << "Member" << id << " ";
  assert(names == expected.str());
  ExpectReply(replies.back(), last_client,
              ":ircserv 366 Member64 #room123 :End of NAMES list");
}

void TestAllocationFailureRollsBackMembershipAndReplies() {
  Clients clients;
  Registration registration("secret", clients);
  Register(clients, registration, 1, "Taro");
  Register(clients, registration, 2, "Hanako");
  Channels channels(clients);
  SendJoin(channels, 1, "JOIN #room");

  IrcMessage message;
  assert(parseMessage("JOIN #room,#second", message));
  const OutgoingMessage previous(1, "previous reply");
  std::vector<OutgoingMessage> replies(1, previous);

  for (int allocation = 0; allocation < 256; ++allocation) {
    allocations_before_failure = allocation;
    const bool succeeded = channels.HandleJoin(2, message, replies);
    allocations_before_failure = -1;
    if (succeeded) {
      assert(replies.size() == 7);
      ExpectReply(replies[2], 2, ":ircserv 353 Hanako = #room :@Taro Hanako");
      ExpectReply(replies[5], 2, ":ircserv 353 Hanako = #second :@Hanako");
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
