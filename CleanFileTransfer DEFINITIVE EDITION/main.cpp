#include <Socket/socket.h>
#include <Console/console.h>

#include "synced.h"
#include "display.h"

#include <future>

using namespace Lunaris;

constexpr u_short DEFINED_PORT = 6676;
const std::string time_date_now = __TIMESTAMP__;

int main()
{
	DisplayCMD* dcmd = nullptr; // created once only, no worries
	std::shared_ptr<SyncedControl> sync;
	TCP_client client;
	std::future<void> async_host;
	bool ref_die_host = false;
	
	int opt = 0;

	dcmd = new DisplayCMD(
		[&] {
			std::shared_ptr<SyncedControl> cpysync = sync;
			if (cpysync) return (client.has_socket() ? "Connected!" : "Offline.") + std::string(" - ") + (opt == 1 ? "via /host" : "via /connect") + " - " + std::to_string(cpysync->get_pack_count()) + " pack(s) - " + std::to_string(cpysync->get_remaining()) + " remaining";
			else return std::string("Waiting setup...");
		},
		[&](const std::string& inp) {
			if (inp.length() && inp[0] == '/')
			{
				if (inp.find("/send ") == 0)
				{
					if (inp.size() <= sizeof("/send")) {
						dcmd->push_message("[CMD] Invalid command arguments.");
					}
					else if (!sync)
					{
						dcmd->push_message("[CMD] Not connected!");
					}
					else {
						const std::string fpnam = inp.substr(sizeof("/send"));
						dcmd->push_message("[CMD] Trying to send '" + fpnam + "'...");

						if (!sync->send_file(fpnam)) {
							dcmd->push_message("[CMD] Failed to queue '" + fpnam + "' to send. Invalid file or file in progress already.");
						}
						else {
							dcmd->push_message("[CMD] Sending file soon!");
						}
					}
				}
				else if (inp.find("/exit") == 0)
				{
					dcmd->set_end();
				}
				else if (inp.find("/build") == 0)
				{
					dcmd->push_message("[CMD] Compiled at " + time_date_now + " GMT-3 Brazil/Sao_Paulo");
				}
				else if (inp.find("/disconnect") == 0)
				{
					if (sync) {
						dcmd->push_message("[CMD] Disconnecting...");
						sync.reset();
						client.close_socket();
						dcmd->push_message("[CMD] Disconnected.");
					}
					else dcmd->push_message("[CMD] Already disconnected.");
				}
				else if (inp.find("/connect ") == 0)
				{
					if (inp.size() <= sizeof("/connect")) {
						dcmd->push_message("[CMD] Invalid command arguments.");
					}
					else {

						if (async_host.valid() && async_host.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
						{
							dcmd->push_message("[CMD] There's an async task already going on. Wait for its end before trying new connections.");
							return 0;
						}
						if (async_host.valid()) async_host.get();

						const std::string ip = inp.substr(sizeof("/connect"));
						dcmd->push_message("[CMD] Connecting to '" + ip + "' asynchronously...");

						async_host = std::async(std::launch::async, [&sync, &dcmd, &client, ip] {
							if (sync) {
								dcmd->push_message("[CMD/ASYNC] Disconnecting from existing connection first...");
								sync.reset();
								client.close_socket();
							}

							if (!client.setup(socket_config().set_ip_address(ip).set_port(DEFINED_PORT)))
							{
								dcmd->push_message("[CMD/ASYNC] Can't connect to '" + ip + "'! Sorry.");
							}
							else {
								dcmd->push_message("[CMD/ASYNC] Connected successfully! Setting up...");

								sync = std::make_shared<SyncedControl>(client, [&](const SyncedControl::message_type& mt, const std::string& str) {
									if (!dcmd) return;
									if (mt == SyncedControl::message_type::MESSAGE) dcmd->push_message("[MSG] Them: " + str);
									else dcmd->push_message("[SYSTEM] " + str);
								});

								dcmd->push_message("[CMD/ASYNC] Ready.");
							}
						});						
					}
				}
				else if (inp.find("/host") == 0)
				{
					if (async_host.valid() && async_host.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
					{
						dcmd->push_message("[CMD] There's an async task already going on. Wait for its end before trying new connections.");
						return 0;
					}
					if (async_host.valid()) async_host.get();

					dcmd->push_message("[CMD] Starting host asynchronously...");

					async_host = std::async(std::launch::async, [&sync, &ref_die_host, &dcmd, &client] {
						if (sync) {
							dcmd->push_message("[CMD] Disconnecting from existing connection first...");
							sync.reset();
							client.close_socket();
						}

						dcmd->push_message("[CMD] Preparing to listen port...");

						TCP_host temphost;
						if (!temphost.setup(socket_config().set_port(DEFINED_PORT))) {
							dcmd->push_message("Failed to setup host. Please allow the app to open a host then try again!");
							return;
						}

						dcmd->push_message("[CMD] Listening for new connection...");

						auto nowtt = std::chrono::system_clock::now() + std::chrono::seconds(10);
						do {
							if (std::chrono::system_clock::now() > nowtt) {
								dcmd->push_message("[CMD] Still waiting for someone to connect...");
								nowtt = std::chrono::system_clock::now() + std::chrono::seconds(10);
							}
							else std::this_thread::sleep_for(std::chrono::seconds(1));

							if (ref_die_host) {
								dcmd->push_message("[CMD] Aborting host! Quit called!");
								return;
							}
						} while ((client = temphost.listen(1)).empty());

						dcmd->push_message("[CMD] Connected successfully! Setting up...");

						sync = std::make_shared<SyncedControl>(client, [&](const SyncedControl::message_type& mt, const std::string& str) {
							if (!dcmd) return;
							if (mt == SyncedControl::message_type::MESSAGE) dcmd->push_message("[MSG] Them: " + str);
							else dcmd->push_message("[SYSTEM] " + str);
							});

						dcmd->push_message("[CMD] Ready.");
					});
				}
				else { // help
					if (inp.find("/help") != 0) dcmd->push_message("[CMD] I don't know this command, maybe you need this help:");
					dcmd->push_message("[CMD] Quick help:");
					dcmd->push_message("[CMD] /send <file>: Sends a file to the other user at '<file>'");
					dcmd->push_message("[CMD] /exit: Exits the app quickly and cleanly.");
					dcmd->push_message("[CMD] /disconnect: Close current connection (tasks will stop running).");
					dcmd->push_message("[CMD] /connect: Connect to a new host. This disconnects automatically.");
					dcmd->push_message("[CMD] /host: Host and listen to someone. After new connection, stops listening.");
					dcmd->push_message("[CMD] /build: Show build timestamp.");
					dcmd->push_message("[CMD] <text>: Anything with no '/' is considered a message.");
				}
			}
			else {
				if (sync) {
					sync->send_message(inp);
					dcmd->push_message("[MSG] You: " + inp);
				}
				else {
					dcmd->push_message("[CMD] Error: not connected.");
				}
			}
		}
	);

	dcmd->push_message("[SYSTEM]           --> CleanFileTransfer DEFINITIVE EDITION <--");
	dcmd->push_message("[SYSTEM]    # A revamped version for quick and easy use by Lohk 2022 #");
	dcmd->push_message("[SYSTEM] %% Compiled at " + time_date_now + " GMT-3 Brazil/Sao_Paulo %%");
	dcmd->push_message("[SYSTEM] - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -");
	dcmd->push_message("[SYSTEM] Hello! Please type '/host' or '/connect <ip>' to host or connect to a host.");
	dcmd->push_message("[SYSTEM] Note: /connect or /host will disconnect any connection going on (if you do more than once).");
	dcmd->push_message("[SYSTEM] Try /help for help (or any invalid command).");

	dcmd->yield_draw();
	ref_die_host = true;

	return 0;
}