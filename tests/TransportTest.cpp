#include "Transport.hpp"

#include <iostream>
#include <stdexcept>
#include <string>

#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

namespace
{
	const unsigned short kPort = 16667;
	const int kClientCount = 3;

	void Check(bool success, const char *message)
	{
		if (!success)
			throw std::runtime_error(message);
	}
}

int main()
{
	int client_fds[kClientCount] = {-1, -1, -1};
	int status = 0;
	try
	{
		Transport transport(kPort);

		// 同じポートで2つ目のサーバーは起動できない。
		bool rejected = false;
		try
		{
			Transport duplicate(kPort);
		}
		catch (const std::runtime_error &e)
		{
			rejected = std::string(e.what()).find("bind:") == 0;
		}
		Check(rejected, "使用中ポートが拒否されなかった");
		std::cout << "PASS: 使用中ポートを拒否" << std::endl;

		// このPCのサーバーへ、3つのクライアントsocketを接続する。
		sockaddr_in address = {};
		address.sin_family = AF_INET;
		address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
		address.sin_port = htons(kPort);
		for (int i = 0; i < kClientCount; ++i)
		{
			client_fds[i] = socket(AF_INET, SOCK_STREAM, 0);
			Check(client_fds[i] != -1, "クライアントsocketを作成できない");
			Check(connect(client_fds[i], reinterpret_cast<sockaddr *>(&address),
				sizeof(address)) == 0, "サーバーへ接続できない");
		}

		// 受付ごとに接続イベントが返り、IDが1、2、3と発行される。
		for (int i = 0; i < kClientCount; ++i)
		{
			const std::vector<TransportEvent> events = transport.WaitForEvents();
			Check(events.size() == 1, "イベント数が1ではない");
			Check(events[0].type == TransportEvent::kConnected,
				"接続イベントではない");
			Check(events[0].client_id == static_cast<ClientId>(i + 1),
				"ClientIdが連番ではない");
		}
		std::cout << "PASS: 3接続のイベントとClientId" << std::endl;
	}
	catch (const std::exception &e)
	{
		std::cerr << "FAIL: " << e.what() << std::endl;
		status = 1;
	}
	for (int i = 0; i < kClientCount; ++i)
	{
		if (client_fds[i] != -1)
			close(client_fds[i]);
	}
	return status;
}
