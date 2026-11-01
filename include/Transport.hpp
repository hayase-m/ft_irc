#ifndef TRANSPORT_HPP
#define TRANSPORT_HPP

#include <map>
#include <vector>

typedef unsigned int ClientId;

struct TransportEvent
{
	enum Type
	{
		kConnected
	};

	Type type;
	ClientId client_id;
};

class Transport
{
public:
	explicit Transport(unsigned short port);
	~Transport();

	std::vector<TransportEvent> WaitForEvents();

private:
	int listen_fd_;
	std::map<ClientId, int> client_fds_;
	ClientId next_client_id_;

	Transport(const Transport &other);
	Transport &operator=(const Transport &other);
};

#endif
