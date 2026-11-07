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
	for (std::map<ClientId, int>::const_iterator it = client_fds_.begin();
		it != client_fds_.end(); ++it)
		close(it->second);
	close(listen_fd_);
}

std::vector<TransportEvent> Transport::WaitForEvents()
{
	std::vector<TransportEvent> events;
	pollfd listener;
	listener.fd = listen_fd_;
	listener.events = POLLIN;
	listener.revents = 0;
	if (poll(&listener, 1, -1) == -1)
	{
		if (errno == EINTR)
			return events;
		ThrowSocketError("poll");
	}
	if (listener.revents & (POLLERR | POLLHUP | POLLNVAL))
		throw std::runtime_error("listening socket is unavailable");
	if (!(listener.revents & POLLIN))
		return events;
	const int client_fd = accept(listen_fd_, NULL, NULL);
	if (client_fd == -1)
	{
		if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR
			|| errno == ECONNABORTED)
			return events;
		ThrowSocketError("accept");
	}
	if (next_client_id_ == kExhaustedClientId)
	{
		close(client_fd);
		return events;
	}
	try
	{
		if (fcntl(client_fd, F_SETFL, O_NONBLOCK) == -1)
			ThrowSocketError("fcntl");
		client_fds_.insert(std::make_pair(next_client_id_, client_fd));
	}
	catch (...)
	{
		close(client_fd);
		throw;
	}
	TransportEvent event;
	event.type = TransportEvent::kConnected;
	event.client_id = next_client_id_++;
	events.push_back(event);
	return events;
}
