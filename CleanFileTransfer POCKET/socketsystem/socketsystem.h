#pragma once

/*
= - = - = - = - = - = - = - = - = - = - = - = - = - = - = - = - = - = - = - = - = - = - = - = - = - = - = - = - = - =
SocketSystem header:

- SocketSystem is a TCP based base (yes) that actually can handle any type of data between two points (host & client)
- It has quick "lost connection" because of the unstoppable pinging (not exactly CPU hungry)
- It's kinda "Full Duplex"
- It has buffer to handle packets like a charm.

= - = - = - = - = - = - = - = - = - = - = - = - = - = - = - = - = - = - = - = - = - = - = - = - = - = - = - = - = - =
*/

#undef UNICODE
#define WIN32_LEAN_AND_MEAN

//#define LSW_EXPERIMENTAL_BOOST

// C
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdlib.h>
#include <stdio.h>
// C++
#include <vector>
#include <thread>
#include <string>
#include <mutex>
#include <functional>

// import
#include "..\abort\abort.h"
#include "..\tools\tools.h"
#include "..\filesystem\filesystem.h"
#include "..\supermutex\supermutex.h"

// ALT
// Need to link with Ws2_32.lib, Mswsock.lib, and Advapi32.lib
#pragma comment (lib, "Ws2_32.lib")
#pragma comment (lib, "Mswsock.lib")
#pragma comment (lib, "AdvApi32.lib")

namespace LSW {
	namespace v5 {
		namespace Sockets {

			constexpr int default_port = 42069;
			constexpr size_t default_package_size = 1 << 13; // 8 kB
			constexpr size_t connection_speed_delay = 1000 / 10; // 10 packages per sec
			constexpr int connection_timeout_speed = 10000; // 10 sec
			constexpr size_t max_buf = 20;

			const std::string default_hello = "LSW_connection_verification";

			enum class internet_way {DOWNLOAD, UPLOAD};

			struct __internal_package {
				char data[default_package_size] = { 0 };
				int data_len = 0;
				int data_type = 0;
				int combine_with_n_more = 0;
			};

			struct final_package {
				string_sized variable_data;
				int data_type = 0;
				final_package() = default;
				final_package(string_sized&& sz, const int dt) { variable_data.reserve(sz.size()); memcpy_s(variable_data.data(), sz.size(), sz.data(), sz.size()); variable_data._set_size(sz.size()); data_type = dt; }
				final_package(char* full, const size_t l, const int dt) { variable_data.reserve(l); memcpy_s(variable_data.data(), l, full, l); variable_data._set_size(l); data_type = dt; }
			};


			class connection_core {
				WSADATA wsaData = WSADATA();
				addrinfo* result = nullptr;
				bool failure = false, init = false;
			public:
				bool initialize(const char*, const int = default_port, const int = 0); // ip (can be null for server), port, ipv6? -1 for client
				bool as_client(SOCKET&);
				bool as_host(SOCKET&);
			};

			// actual stuff used

			class con_client {
				connection_core core;

				SOCKET Connected = INVALID_SOCKET;

				std::thread* alive_sending_part = nullptr;
				std::thread* alive_recving_part = nullptr;
				std::thread* connection_monitor = nullptr;

				ULONGLONG dt_mon_tks = 0;
				bool mon_cons_all = false;
				size_t sending_ticks = 0, _snd_t = 0;
				size_t recving_ticks = 0, _rcv_t = 0;

				bool die = false;
				//bool unlocked = false;
				size_t upload_delay = connection_speed_delay;
				size_t download_delay = connection_speed_delay;

				std::vector<__internal_package> received, sending;
				SuperMutex received_hold, sending_hold;

				std::function<void(const std::string)> prunt = std::function<void(const std::string)>();

				void ___lock_s();
				void ___unlock_s();
				void ___lock_r();
				void ___unlock_r();

				void __keep_monitoring();

				void __tick_send();
				void __tick_recv();

				//void __keep_connection_alive();
				void __keep_receive();
				void __keep_sending();

				// later: thread keeping alive, std::vector to buffer send/recv, bool to tell if still up

				bool _send_raw(void*, int);
				bool _recv_raw(void*, int);

				bool send_blank();

				bool send_package(__internal_package&);
				bool recv_package(__internal_package&);

				//void start_internally_as_client();   // as client
				//void start_internally_as_host(); // as host
				void start_internally_universal(); // both ;P

			public:
				con_client(SOCKET = INVALID_SOCKET); // custom non-verified call (probably from host)
				~con_client();

				bool connect(const char* = "127.0.0.1", const int = default_port); // normally client call this one

				void kill_connection(); // only if you really want to die
				bool still_on();
				size_t hasPackage();
				size_t hasSending();
				// packages per second
				void setSpeed(const internet_way, const size_t = connection_speed_delay);
				// if hookPrint, show total data (consider empty) or only actual data (only your data)?
				void considerEmptyPackagesOnDataFlow(const bool);

				//bool send_nolock(final_package);
				void send(final_package&&);
				bool recv(final_package&);

				void hookPrint(std::function<void(const std::string)>);
			};

			class con_host {
				connection_core core;

				SOCKET Listening = INVALID_SOCKET;

				std::vector<con_client*> connections;
				std::mutex connections_m;
				size_t max_connections_allowed = 1;

				std::thread* listener = nullptr;
				std::thread* autodisconnect = nullptr;
				bool still_running = false;

				std::function<void(const std::string)> prunt = std::function<void(const std::string)>();

				void auto_accept();
				void auto_cleanup();

				void _initialize(const char*, const int, const bool);
			public:
				con_host(const int, const bool = false);
				con_host(const bool = false);
				~con_host();

				void lock();
				void unlock();
				std::vector<con_client*>::iterator begin();
				std::vector<con_client*>::iterator end();
				size_t size();

				void setMaxConnections(const size_t);

				void hookPrint(std::function<void(const std::string)>);
			};
		}
	}
}