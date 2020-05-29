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
#include <memory>

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
			constexpr size_t default_package_size = 1 << 16; // was << 16 64 kB
			//constexpr size_t default_package_size = 65; // test
			//constexpr size_t connection_speed_delay = 1000 / 10; // 10 packages per sec
			constexpr int connection_timeout_speed = 1000; // 1.0 sec, also used on recv if huge package is incomplete and it waits the recv to get what is left
			constexpr size_t max_buf = 50;


			enum class internet_way {DOWNLOAD, UPLOAD};

			struct __internal_package {
				char data[default_package_size] = { 0 };
				int data_len = 0;
				int data_type = 0;
				int combine_with_n_more = 0;
				size_t package_id = 0;
			};

			struct final_package {
				std::string variable_data;
				int data_type = 0;

				inline final_package() = default;
				inline final_package(const std::string& sz, const int& dt) : variable_data(sz), data_type(dt) { }
				inline final_package(const char* full, const size_t l, const int dt) : variable_data(full, l), data_type(dt) { }
				inline bool operator==(final_package& pkg) { return ((variable_data.compare(0, variable_data.size(), pkg.variable_data.data(), pkg.variable_data.size()) == 0) && data_type == pkg.data_type); }
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

				std::thread* thr_recv = nullptr; bool thr_recv_running = false;
				std::thread* thr_send = nullptr; bool thr_send_running = false;
				std::thread* thr_mont = nullptr; bool thr_mont_running = false;

				size_t task_recv = 0, end_task_recv = 0;
				size_t task_send = 0, end_task_send = 0;
				size_t task_totl_recv = 0, task_totl_send = 0;
				size_t package_loss_recv = 0, end_package_loss_recv = 0;
				size_t package_loss_send = 0, end_package_loss_send = 0;
				std::chrono::milliseconds latest_update = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch());

				std::vector<std::shared_ptr<final_package>> sending, received;
				std::mutex sending_m, received_m;
				Waiter wait_recv, send_wait, recv_overflow_wait;
				bool kill_threads = false;
				size_t total_sockets_sr = 0;
				size_t total_sockets_sending_only = 0;

				std::function<void(std::string)> bw_prunt, nt_prunt;

				//void __task_recv();
				//void __task_send();
				void __ploss_recv();
				void __ploss_send();

				bool __send_small(__internal_package&);
				bool __recv_small(__internal_package&);

				void __thr_send();
				void __thr_recv();
				void __thr_mont();

				void start_threads();

				void __set_kill();
			public:
				con_client(SOCKET = INVALID_SOCKET); // custom non-verified call (probably from host)
				~con_client();

				bool connect(const char* = "127.0.0.1", const int = default_port); // normally client call this one

				void kill(); // only if you really want to die
				bool isConnected();
				size_t hasPackage();
				size_t hasSending();

				void send(std::string, const int);
				void send(std::shared_ptr<final_package>);
				bool recv_nolock(std::shared_ptr<final_package>&);
				// size_t = maximum time to wait, 0 being inf, millisec
				bool recv(std::shared_ptr<final_package>&, const size_t = 0);

				bool hadEventRightNow(const size_t = connection_timeout_speed);

				void hookPrintBandwidth(std::function<void(const std::string)>);
				void hookPrintEvent(std::function<void(const std::string)>);

				size_t getSendTrafficPerSec();
				size_t getRecvTrafficPerSec();
				size_t getSendTrafficTotal();
				size_t getRecvTrafficTotal();
				size_t getTotalTraffic();
			};

			class con_host {
				connection_core core;

				SOCKET Listening = INVALID_SOCKET;

				std::vector<con_client*> connections;
				SuperMutex connections_m;
				size_t max_connections_allowed = 1;

				Waiter wait_new_connection;

				std::thread* listener = nullptr;
				std::thread* autodisconnect = nullptr;
				bool still_running = false;

				std::function<void(const std::string)> bw_prunt = std::function<void(const std::string)>();

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

				con_client* waitNewConnection(const size_t = 0);

				void hookPrintBandwidth(std::function<void(const std::string)>);
			};
		}
	}
}