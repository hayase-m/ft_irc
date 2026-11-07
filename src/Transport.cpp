#include "Transport.hpp"

#include <cerrno>
#include <cstring>
#include <stdexcept>
#include <string>

#include <fcntl.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

namespace
{
	const ClientId kExhaustedClientId = 0;

	void ThrowSocketError(const std::string &operation)
	{
		throw std::runtime_error(operation + ": "
			+ std::strerror(errno));
	}

	void AddEvent(std::vector<TransportEvent> &events,
		TransportEvent::Type type, ClientId client_id)
	{
		TransportEvent event;
		event.type = type;
		event.client_id = client_id;
		events.push_back(event);
	}
}

Transport::Transport(unsigned short port)
	: listen_fd_(-1), next_client_id_(1)
{
	listen_fd_ = socket(AF_INET, SOCK_STREAM, 0);
	if (listen_fd_ == -1)
		ThrowSocketError("socket");
	try
	{
		if (fcntl(listen_fd_, F_SETFL, O_NONBLOCK) == -1)
			ThrowSocketError("fcntl");
		const int yes = 1;
		if (setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR,
			&yes, sizeof(yes)) == -1)
			ThrowSocketError("setsockopt");
		sockaddr_in address = {};
		address.sin_family = AF_INET;
		address.sin_addr.s_addr = htonl(INADDR_ANY);
		address.sin_port = htons(port);
		if (bind(listen_fd_, reinterpret_cast<sockaddr *>(&address),
			sizeof(address)) == -1)
			ThrowSocketError("bind");
		if (listen(listen_fd_, SOMAXCONN) == -1)
			ThrowSocketError("listen");
	}
	catch (...)
	{
		close(listen_fd_);
		throw;
	}
}

Transport::~Transport()
{
	for (std::map<ClientId, Client>::const_iterator it = clients_.begin();
		it != clients_.end(); ++it)
		close(it->second.fd);
	close(listen_fd_);
}

std::vector<TransportEvent> Transport::WaitForEvents()
{
	std::vector<TransportEvent> events;
	if (!disconnected_events_.empty())
	{
		events.swap(disconnected_events_);
		return events;
	}
	std::vector<pollfd> poll_fds;
	pollfd listener = {listen_fd_, POLLIN, 0};
	poll_fds.push_back(listener);
	for (std::map<ClientId, Client>::iterator it = clients_.begin();
		it != clients_.end(); ++it)
	{
		it->second.read_ready = false;
		pollfd client = {it->second.fd, POLLIN, 0};
		if (it->second.wants_to_send)
			client.events |= POLLOUT;
		poll_fds.push_back(client);
	}
	if (poll(&poll_fds[0], poll_fds.size(), -1) == -1)
	{
		if (errno == EINTR)
			return events;
		ThrowSocketError("poll");
	}
	if (poll_fds[0].revents & (POLLERR | POLLHUP | POLLNVAL))
		throw std::runtime_error("listening socket is unavailable");
	std::size_t index = 1;
	std::map<ClientId, Client>::iterator it = clients_.begin();
	while (it != clients_.end())
	{
		std::map<ClientId, Client>::iterator current = it++;
		const short ready = poll_fds[index++].revents;
		if ((ready & (POLLERR | POLLNVAL))
			|| ((ready & POLLHUP) && !(ready & POLLIN)))
		{
			Disconnect(current->first);
			continue;
		}
		if (ready & POLLIN)
		{
			current->second.read_ready = true;
			AddEvent(events, TransportEvent::kReadable, current->first);
		}
		if ((ready & POLLOUT) && !(ready & POLLHUP))
			AddEvent(events, TransportEvent::kWritable, current->first);
	}
	events.insert(events.end(), disconnected_events_.begin(),
		disconnected_events_.end());
	disconnected_events_.clear();
	if (poll_fds[0].revents & POLLIN)
		AcceptClient(events);
	return events;
}

void Transport::AcceptClient(std::vector<TransportEvent> &events)
{
	const int client_fd = accept(listen_fd_, NULL, NULL);
	if (client_fd == -1)
		return;
	if (next_client_id_ == kExhaustedClientId)
	{
		close(client_fd);
		return;
	}
	if (fcntl(client_fd, F_SETFL, O_NONBLOCK) == -1)
	{
		close(client_fd);
		return;
	}
	try
	{
		Client client = {client_fd, false, false};
		clients_.insert(std::make_pair(next_client_id_, client));
	}
	catch (...)
	{
		close(client_fd);
		throw;
	}
	AddEvent(events, TransportEvent::kConnected, next_client_id_++);
}

bool Transport::Receive(ClientId client_id, std::string &data)
{
	data.clear();
	std::map<ClientId, Client>::iterator it = clients_.find(client_id);
	if (it == clients_.end())
		return false;
	if (!it->second.read_ready)
		throw std::logic_error("Receive requires a readable event");
	it->second.read_ready = false;
	char buffer[4096];
	const ssize_t received = recv(it->second.fd, buffer, sizeof(buffer), 0);
	if (received <= 0)
	{
		Disconnect(client_id);
		return false;
	}
	data.assign(buffer, static_cast<std::size_t>(received));
	return true;
}

void Transport::SetWriteInterest(ClientId client_id, bool enabled)
{
	std::map<ClientId, Client>::iterator it = clients_.find(client_id);
	if (it != clients_.end())
		it->second.wants_to_send = enabled;
}

void Transport::Disconnect(ClientId client_id)
{
	std::map<ClientId, Client>::iterator it = clients_.find(client_id);
	if (it == clients_.end())
		return;
	AddEvent(disconnected_events_, TransportEvent::kDisconnected, client_id);
	close(it->second.fd);
	clients_.erase(it);
}
