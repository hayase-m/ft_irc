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

	void Check(bool success, const char *message)
	{
		if (!success)
			throw std::runtime_error(message);
	}

	bool HasEvent(const std::vector<TransportEvent> &events,
		TransportEvent::Type type, ClientId client_id)
	{
		for (std::size_t i = 0; i < events.size(); ++i)
		{
			if (events[i].type == type && events[i].client_id == client_id)
				return true;
		}
		return false;
	}

	// テスト用クライアント。サーバー側のFDにはアクセスしない。
	class Peer
	{
	public:
		Peer() : fd_(socket(AF_INET, SOCK_STREAM, 0))
		{
			Check(fd_ != -1, "クライアントsocketを作成できない");
		}

		~Peer() { Close(); }

		void StartConnection()
		{
			sockaddr_in address = {};
			address.sin_family = AF_INET;
			address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
			address.sin_port = htons(kPort);
			Check(connect(fd_, reinterpret_cast<sockaddr *>(&address),
				sizeof(address)) == 0, "サーバーへ接続できない");
		}

		ClientId Connect(Transport &transport)
		{
			StartConnection();
			const std::vector<TransportEvent> events = transport.WaitForEvents();
			Check(events.size() == 1
				&& events[0].type == TransportEvent::kConnected,
				"接続イベントが1件ではない");
			return events[0].client_id;
		}

		void Send(const std::string &data)
		{
			std::size_t offset = 0;
			while (offset < data.size())
			{
				const ssize_t sent = send(fd_, data.data() + offset,
					data.size() - offset, 0);
				Check(sent > 0, "クライアントから送信できない");
				offset += static_cast<std::size_t>(sent);
			}
		}

		void Close()
		{
			if (fd_ != -1)
				close(fd_);
			fd_ = -1;
		}

		void Reset()
		{
			linger option = {1, 0};
			Check(setsockopt(fd_, SOL_SOCKET, SO_LINGER, &option,
				sizeof(option)) == 0, "RST送信を設定できない");
			Close();
		}

	private:
		int fd_;
		Peer(const Peer &other);
		Peer &operator=(const Peer &other);
	};

	std::string ReadBytes(Transport &transport, ClientId client_id,
		std::size_t count)
	{
		std::string result;
		while (result.size() < count)
		{
			const std::vector<TransportEvent> events = transport.WaitForEvents();
			Check(HasEvent(events, TransportEvent::kReadable, client_id),
				"読み取り可能が通知されなかった");
			std::string data;
			Check(transport.Receive(client_id, data), "データ受信前に切断された");
			result += data;
		}
		Check(result.size() == count, "受信サイズが一致しない");
		return result;
	}

	void ExpectDisconnected(Transport &transport, ClientId client_id)
	{
		while (true)
		{
			const std::vector<TransportEvent> events = transport.WaitForEvents();
			if (HasEvent(events, TransportEvent::kDisconnected, client_id))
				return;
			Check(HasEvent(events, TransportEvent::kReadable, client_id),
				"切断またはEOFを受信できない");
			std::string data = "old data";
			Check(!transport.Receive(client_id, data) && data.empty(),
				"EOFで切断されなかった");
		}
	}

	void TestNotifications()
	{
		Transport transport(kPort);
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
		Peer first;
		const ClientId first_id = first.Connect(transport);
		Peer second;
		const ClientId second_id = second.Connect(transport);
		Peer third;
		const ClientId third_id = third.Connect(transport);
		Check(first_id == 1 && second_id == 2 && third_id == 3,
			"ClientIdが連番ではない");

		std::string data;
		rejected = false;
		try
		{
			transport.Receive(first_id, data);
		}
		catch (const std::logic_error &)
		{
			rejected = true;
		}
		Check(rejected, "読み取り可能通知の前にReceiveできた");

		first.Send("com");
		Check(ReadBytes(transport, first_id, 3) == "com", "分割入力の前半が違う");
		const std::string remainder("mand\r\nNEXT\r\n\0x", 14);
		first.Send(remainder);
		Check(ReadBytes(transport, first_id, remainder.size()) == remainder,
			"複数行またはNULを含むデータが変化した");
		rejected = false;
		try
		{
			transport.Receive(first_id, data);
		}
		catch (const std::logic_error &)
		{
			rejected = true;
		}
		Check(rejected, "同じ読み取り可能通知でReceiveを繰り返せた");

		transport.SetWriteInterest(second_id, true);
		std::vector<TransportEvent> events = transport.WaitForEvents();
		Check(events.size() == 1
			&& HasEvent(events, TransportEvent::kWritable, second_id),
			"指定した接続だけが書き込み可能にならなかった");
		second.Send("both");
		bool received = false;
		while (!received)
		{
			events = transport.WaitForEvents();
			Check(HasEvent(events, TransportEvent::kWritable, second_id),
				"書き込み監視が通知前に解除された");
			if (HasEvent(events, TransportEvent::kReadable, second_id))
			{
				Check(transport.Receive(second_id, data) && data == "both",
					"読み書き可能が同時に通知されたときに受信できない");
				received = true;
			}
		}
		transport.SetWriteInterest(second_id, false);
		first.Send("a");
		third.Send("c");
		bool first_read = false;
		bool third_read = false;
		while (!first_read || !third_read)
		{
			events = transport.WaitForEvents();
			Check(!HasEvent(events, TransportEvent::kWritable, second_id),
				"書き込み監視を解除できない");
			for (std::size_t i = 0; i < events.size(); ++i)
			{
				Check(events[i].type == TransportEvent::kReadable,
					"読み取り可能以外のイベントが返った");
				Check(transport.Receive(events[i].client_id, data), "受信できない");
				if (events[i].client_id == first_id)
					first_read = data == "a";
				else if (events[i].client_id == third_id)
					third_read = data == "c";
				else
					Check(false, "受信元ClientIdが違う");
			}
		}

		// peerのFDを先に確保し、解放するサーバーFDをacceptで再利用させる。
		Peer replacement;
		transport.Disconnect(first_id);
		transport.Disconnect(first_id);
		events = transport.WaitForEvents();
		Check(events.size() == 1
			&& HasEvent(events, TransportEvent::kDisconnected, first_id),
			"切断通知が重複した");
		const ClientId replacement_id = replacement.Connect(transport);
		Check(replacement_id > third_id, "切断したClientIdが再利用された");
		transport.SetWriteInterest(first_id, true);
		transport.Disconnect(first_id);
		Check(!transport.Receive(first_id, data), "古いClientIdで受信できた");
		replacement.Send("new");
		events = transport.WaitForEvents();
		Check(events.size() == 1
			&& HasEvent(events, TransportEvent::kReadable, replacement_id),
			"古いClientIdの操作が新しい接続へ影響した");
		Check(transport.Receive(replacement_id, data) && data == "new",
			"再接続先から受信できない");
		std::cout << "PASS: 接続・受信・監視切替・FD再利用" << std::endl;
	}

	void TestDisconnects()
	{
		Transport transport(kPort);
		Peer first;
		const ClientId first_id = first.Connect(transport);
		Peer survivor;
		const ClientId survivor_id = survivor.Connect(transport);
		first.Close();
		ExpectDisconnected(transport, first_id);

		Peer with_data;
		const ClientId data_id = with_data.Connect(transport);
		const std::string payload(12000, 'x');
		with_data.Send(payload);
		with_data.Close();
		Check(ReadBytes(transport, data_id, payload.size()) == payload,
			"切断直前のデータが失われた");
		ExpectDisconnected(transport, data_id);

		Peer reset;
		const ClientId reset_id = reset.Connect(transport);
		reset.Reset();
		ExpectDisconnected(transport, reset_id);
		survivor.Send("alive");
		Check(ReadBytes(transport, survivor_id, 5) == "alive",
			"他の接続の切断後に受信できない");
		Peer later;
		Check(later.Connect(transport) > reset_id, "切断後に新規接続できない");
		std::cout << "PASS: EOF・データ送信直後の切断・RST・他接続の継続"
			<< std::endl;
	}

	void TestAcceptWithDisconnect()
	{
		Transport transport(kPort);
		Peer first;
		const ClientId first_id = first.Connect(transport);
		first.Close();
		// 新規接続と既存接続のEOFが続けて到着する。
		Peer incoming;
		incoming.StartConnection();
		ClientId incoming_id = 0;
		bool disconnected = false;
		while (incoming_id == 0 || !disconnected)
		{
			const std::vector<TransportEvent> events = transport.WaitForEvents();
			for (std::size_t i = 0; i < events.size(); ++i)
			{
				if (events[i].type == TransportEvent::kConnected)
					incoming_id = events[i].client_id;
			}
			if (HasEvent(events, TransportEvent::kReadable, first_id))
			{
				std::string data;
				Check(!transport.Receive(first_id, data), "古い接続のEOFを検知できない");
			}
			if (HasEvent(events, TransportEvent::kDisconnected, first_id))
				disconnected = true;
		}
		Check(incoming_id > first_id, "切断と同時に新規接続を受け付けられない");
		incoming.Send("new");
		Check(ReadBytes(transport, incoming_id, 3) == "new",
			"同時に受け付けた接続のデータを受信できない");
		std::cout << "PASS: 接続受付と切断の同時処理" << std::endl;
	}

	void TestRepeatedConnections()
	{
		Transport transport(kPort);
		ClientId previous_id = 0;
		for (int i = 0; i < 128; ++i)
		{
			Peer peer;
			const ClientId client_id = peer.Connect(transport);
			Check(client_id > previous_id, "過去のClientIdが再利用された");
			peer.Close();
			ExpectDisconnected(transport, client_id);
			previous_id = client_id;
		}
		std::cout << "PASS: 128回の接続・切断" << std::endl;
	}
}

int main()
{
	try
	{
		TestNotifications();
		TestDisconnects();
		TestAcceptWithDisconnect();
		TestRepeatedConnections();
	}
	catch (const std::exception &e)
	{
		std::cerr << "FAIL: " << e.what() << std::endl;
		return 1;
	}
	return 0;
}
