#include "Registration.hpp"

#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <iostream>
#include <new>

namespace {

int allocations_before_failure = -1;

void Send(Registration &registration, ClientId id, const std::string &line,
          std::vector<std::string> &replies)
{
    IrcMessage message;
    assert(parseMessage(line, message));
    assert(registration.HandleMessage(id, message, replies));
}

void ExpectCode(const std::vector<std::string> &replies, const std::string &code)
{
    assert(replies.size() == 1);
    assert(replies[0].compare(0, 13, ":ircserv " + code + " ") == 0);
}

void TestAllOrders()
{
    const char *commands[] = {
        "PASS secret", "NICK Taro", "USER taro 0 * :Taro Yamada"
    };
    int order[] = {0, 1, 2};

    do
    {
        Registration registration("secret");
        std::vector<std::string> replies;
        assert(registration.AddClient(1));

        for (int i = 0; i < 3; ++i)
        {
            Send(registration, 1, commands[order[i]], replies);
            assert(registration.FindClient(1)->registered == (i == 2));
            if (i < 2)
                assert(replies.empty());
        }

        ExpectCode(replies, "001");
        assert(registration.FindClient(1)->realname == "Taro Yamada");
        Send(registration, 1, "PASS secret", replies);
        ExpectCode(replies, "462");
        Send(registration, 1, "USER other 0 * :Other Name", replies);
        ExpectCode(replies, "462");
        assert(registration.FindClient(1)->username == "taro");
    } while (std::next_permutation(order, order + 3));
}

void TestErrorsAndRemoval()
{
    Registration registration("secret");
    std::vector<std::string> replies;

    assert(registration.AddClient(1));
    assert(registration.AddClient(2));

    Send(registration, 1, "PASS", replies);
    ExpectCode(replies, "461");
    Send(registration, 1, "PASS wrong", replies);
    ExpectCode(replies, "464");
    assert(!registration.FindClient(1)->authenticated);

    Send(registration, 1, "USER taro", replies);
    ExpectCode(replies, "461");
    Send(registration, 1, "NICK", replies);
    ExpectCode(replies, "431");
    Send(registration, 1, "NICK 123", replies);
    ExpectCode(replies, "432");
    Send(registration, 1, "NICK abcdefghij", replies);
    ExpectCode(replies, "432");
    Send(registration, 1, "NICK Abcdefghi", replies);
    assert(replies.empty());
    assert(!registration.AddClient(1));
    assert(registration.FindClient(1)->nickname == "Abcdefghi");

    Send(registration, 2, "NICK abcdefghi", replies);
    ExpectCode(replies, "433");
    assert(registration.FindClient(2)->nickname.empty());

    registration.RemoveClient(1);
    assert(!registration.FindClient(1));
    Send(registration, 1, "PASS secret", replies);
    assert(!registration.FindClient(1));
    assert(replies.empty());

    Send(registration, 2, "NICK abcdefghi", replies);
    assert(registration.FindClient(2)->nickname == "abcdefghi");
    assert(replies.empty());
}

void TestAllocationFailures()
{
    Registration registration("secret");
    allocations_before_failure = 0;
    const bool added = registration.AddClient(1);
    allocations_before_failure = -1;
    assert(!added);
    assert(!registration.FindClient(1));
    assert(registration.AddClient(1));

    std::vector<std::string> replies;
    Send(registration, 1, "PASS secret", replies);
    Send(registration, 1, "NICK Taro", replies);
    replies.push_back("previous reply");

    IrcMessage user;
    assert(parseMessage(
        "USER taro 0 * :A real name long enough to require an allocation",
        user));

    for (int allocation = 0; allocation < 64; ++allocation)
    {
        allocations_before_failure = allocation;
        const bool handled = registration.HandleMessage(1, user, replies);
        allocations_before_failure = -1;

        const ClientState *state = registration.FindClient(1);
        if (handled)
        {
            assert(state->registered);
            assert(state->username == "taro");
            assert(state->realname == user.params[3]);
            ExpectCode(replies, "001");
            return;
        }

        assert(state->authenticated);
        assert(!state->registered);
        assert(state->nickname == "Taro");
        assert(state->username.empty());
        assert(state->realname.empty());
        assert(replies.size() == 1 && replies[0] == "previous reply");
    }

    assert(false);
}

} // namespace

void *operator new(std::size_t size) throw(std::bad_alloc)
{
    if (allocations_before_failure == 0)
        throw std::bad_alloc();
    if (allocations_before_failure > 0)
        --allocations_before_failure;

    void *memory = std::malloc(size == 0 ? 1 : size);
    if (memory == NULL)
        throw std::bad_alloc();
    return memory;
}

void operator delete(void *memory) throw()
{
    std::free(memory);
}

int main()
{
    TestAllOrders();
    TestErrorsAndRemoval();
    TestAllocationFailures();
    std::cout << "Registration tests passed" << std::endl;
}
