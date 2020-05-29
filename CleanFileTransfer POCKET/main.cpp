#include <iostream>
#include <stdlib.h>
#include <stdio.h>
#include <string>
#include <mutex>
#include <thread>
#include <vector>
#include <algorithm>

#include "socketsystem/socketsystem.h"
#include "filesystem/filesystem.h"
#include "display_cmd.h"

//#define LSW_DEBUG

#ifdef LSW_DEBUG
#define DEBUG_LINES 8
#endif

using namespace LSW::v5;

const std::string version_app = "CleanFileTransfer POCKET beta 0.6.1b";
const std::string discord_link = std::string("ht") + std::string("tps://discord.gg/a5G") + "GgBt";

enum class packages_id {VERSION_CHECK=1, MESSAGE, MESSAGE_SENT, PASSWORD_CHECK, FILE_AVAILABLE, FILE_RECEIVING, FILE_SIZE, FILE_OPEN, FILE_CLOSE, FILE_OK, FILE_NOT_OK};


struct user_input {
	std::string input_going_on;
	SuperMutex m;
	bool die = false;
	bool hasbreak = false;

	bool isInputFull() { return hasbreak; }
	std::string copyInput() { m.lock(); std::string cpy = input_going_on; m.unlock(); return cpy; }
	void clsInput() { m.lock(); input_going_on.clear(); hasbreak = false; m.unlock(); }
};

// user_input saves user input
void input_handler(user_input&);

// display to update
void update_display(Custom::DISPLAY<100, 30>&, bool&);

// text, limit, time to bounce, specific time to set as init, how many ticks each end
std::string shrinkText(std::string, const size_t, const ULONGLONG, const ULONGLONG = 0, const size_t = 10);

// replaces "/" to "$", "\\" to "%" and ":" to "-"
void cleanAvoidPath(std::string&);

// Automatically sends a file through the con_client. bool for message_sent, Mutex guarantee package read, Bool is for "kill" if needed, second bool is to tell you if it is working on something
void sendFileAuto(bool&, SuperMutex&, bool&, Sockets::con_client*, std::vector<std::pair<std::string, short>>&, bool&, std::string&);
// Handles packages. It will save messages on the vector. bool for message_sent, Mutex guarantee package read, First bool is "kill". Files are saved if second bool is true, else discarded. It dies automatically if connection dies too. Last std::string is for messages from "system"
void handlePackages(bool&, SuperMutex&, bool&, Sockets::con_client*, bool&, std::vector<Sockets::final_package>&, std::string&);


int main() {
	std::cout << "Initializing..." << std::endl;

	user_input input;
	Custom::DISPLAY<100, 30> display;
	Sockets::con_host* host = nullptr;
	Sockets::con_client* client = nullptr;
	std::vector<std::pair<std::string,short>> items_list;
	bool sending = false;
	bool message_sent = false;
	std::vector<Sockets::final_package> packages_coming;
	bool is_file_transfer_enabled = false;
	std::string system_message;
	std::string internal_message;
	SuperMutex transf_m;
	bool always_go_back_to_debug = false;

#ifdef LSW_DEBUG
	std::string lines_log[DEBUG_LINES];
	auto log_last = [&](std::string a = "") {if (a.length() > 0) { for (int u = DEBUG_LINES - 1; u > 0; u--) lines_log[u] = lines_log[u - 1]; lines_log[0] = a; } for (size_t u = 0; u < DEBUG_LINES; u++) display.printAt(display.getLineAmount() - u - 1, lines_log[u]); };
#endif

	std::thread thr_input([&]() { input_handler(input); });
	std::thread thr_autorefresh([&]() { update_display(display, input.die); });
	std::thread* thr_handle_packages = nullptr; // exists if there's a connection
	std::thread* thr_autosend_packages = nullptr; // exists while has something to send

	//thr_handle_packages = new std::thread([&]() {handlePackages(transf_m, input.die, (host ? *host->begin() : client), is_file_transfer_enabled, packages_coming, system_message); }); // if not connected, die.
	//thr_autosend_packages = new std::thread([&]() {sendFileAuto(transf_m, input.die, (host ? *host->begin() : client), items_list, sending, system_message); });

	auto handle_message = [&] {
		if (packages_coming.size() > 0) {
			Sockets::final_package& pkg = packages_coming[0];
			if (pkg.data_type == static_cast<int>(packages_id::MESSAGE)) {
				display.newMessage("[Them -> You] " + std::string(pkg.variable_data.data()));
			}
			packages_coming.erase(packages_coming.begin());
		}
	};

	display.setTitle([&]() {
		std::string buf = version_app + " | L: " + (system_message.length() > 0 ? system_message : "none") + " I: " + (internal_message.length() > 0 ? internal_message : "none") + " ";
		if (host) {
			size_t connected = 0;
			for (auto& i : *host) {
				connected += i->isConnected();
			}
			buf += "| C/T: " + std::to_string(connected) + "/" + std::to_string(host->size());
		}
		if (client) {
			buf += "| C ";
			if (client->isConnected()) buf += "ON";
			else buf += "OFF";
		}
		buf += " | Items: " + std::to_string(items_list.size());

		return shrinkText(buf, 100, 200);
		});

	display.printAt(0, "[@] Hello! Start with 'help' if you don't have any idea how this app works.");
		
	bool clear_input_waiting = false;
	std::string last_input = "";

	for (bool dead = false; !dead;) {
#ifdef LSW_DEBUG
		log_last();
#endif

		Sleep(150);

		handle_message();

		if (input.copyInput() != last_input) {
			last_input = input.copyInput();
			display.setCommandEntry(input.copyInput());
			//display.flip();
		}

		if (input.isInputFull()) {
			std::string buf = input.copyInput();
			clear_input_waiting = true; // input.clsInput(); later is 10/10

			std::vector<std::string> splut;
			{
				bool dont_divide_space = false;
				std::string last_buf;
				for (auto& i : buf) {
					if (i == '\"') {
						dont_divide_space = !dont_divide_space;
					}
					else if ((i != ' ' || dont_divide_space) && i != '\n') {
						last_buf += i;
					}
					else {
						if (last_buf.length() > 0) splut.push_back(last_buf);
						last_buf.clear();
					}
				}
				if (last_buf.length() > 0) splut.push_back(last_buf);
			}

			// handle

			if (splut.size() > 0) {

				std::string cmd = splut[0];

				display.clearAllLines();

				if (cmd == "help" || cmd == "h") {

					if (splut.size() == 1) {
						display.printAt(0, "[@] Help:");
						display.printAt(1, "These are the commands available to use: (try also 'help <any>')");
						display.printAt(2, "- host: start con_host;");
						display.printAt(3, "- connect: start con_client;");
						display.printAt(4, "- add: adds new item to the list to transfer;");
						display.printAt(5, "- del: removes an item from the list to transfer;");
						display.printAt(6, "- show: shows the list to transfer;");
						display.printAt(7, "- receivefiles: set to receive the files;");
						display.printAt(8, "- sendfiles: put the files from the list to transfer;");
						display.printAt(9, "- message: sends a message;");
						display.printAt(10, "- version: shows the version of the app;");
						display.printAt(11, "- discord: shows main discord server link;");
						display.printAt(12, "- tutorial: shows a tutorial if you don't know what to do;");
						display.printAt(13, "- exit: exits the app.");
						display.printAt(13, "- credits: shows credits.");
					}
					else { // expanded help

						std::string what_to_help = splut[1];

						display.printAt(0, "[@] Help to '" + what_to_help + "':");

						if (what_to_help == "help") {
							display.printAt(1, "Help is an useful command to get help of things you can do here.");
							display.printAt(2, "You can also get more detailed help by using 'help' and the command you want help.");
							display.printAt(3, "You are crazy searching for help to help...");
							display.printAt(9, "Command args: 'help <command>'");
						}
						else if (what_to_help == "host") {
							display.printAt(1, "Host turns on the 'Host' part of the app, also known as 'server'.");
							display.printAt(2, "If you enable the Host part, someone can connect to your computer.");
							display.printAt(3, "If you want to connect to someone that has hosted, use 'connect' instead.");
							display.printAt(9, "Command args: 'host [password] <ipv6>'");
						}
						else if (what_to_help == "connect") {
							display.printAt(1, "Connect is the command to connect to someone hosting.");
							display.printAt(2, "Someone can host a server by using the 'host' command.");
							display.printAt(9, "Command args: 'connect [ip] [password]'");
						}
						else if (what_to_help == "add") {
							display.printAt(1, "Adds a new item to the sending list.");
							display.printAt(2, "You can actually DROP the file here (it copies the path for you).");
							display.printAt(9, "Command args: 'add [path]'");
							display.printAt(10, "Alias: 'a'");
						}
						else if (what_to_help == "del") {
							display.printAt(1, "Removes an item from the sending list.");
							display.printAt(9, "Command args: 'del [item(number)]'");
							display.printAt(10, "Alias: 'd'");
						}
						else if (what_to_help == "show") {
							display.printAt(1, "Shows the 'sending list' itself.");
							display.printAt(9, "Command args: 'show <page>'");
							display.printAt(10, "Alias: 's'");
						}
						else if (what_to_help == "receivefiles") {
							display.printAt(1, "Toggles if automatic file receive is available or not.");
							display.printAt(2, "You can force 'disabled' or 'enabled' if you want.");
							display.printAt(2, "If enabled, files sent to you will be saved next to this app.");
							display.printAt(10, "Alias: 'rf'");
						}
						else if (what_to_help == "sendfiles") {
							display.printAt(1, "Start to upload files to whoever is connected (if they are accepting files).");
							display.printAt(10, "Alias: 'sf'");
						}
						else if (what_to_help == "message") {
							display.printAt(1, "Sends a message to whoever is connected on the other side.");
							display.printAt(9, "Command args: 'message <text string>'");
							display.printAt(10, "Alias: 'm'");
						}
						else if (what_to_help == "version") {
							display.printAt(1, "Shows app version.");
						}
						else if (what_to_help == "discord") {
							display.printAt(1, "Shows the server link where you can get help and chat.");
						}
						else if (what_to_help == "tutorial") {
							display.printAt(1, "Shows a little simple tutorial on how to send a file to your friend.");
							display.printAt(2, "If you don't have any idea how this app works, this might help.");
						}
						else if (what_to_help == "exit") {
							display.printAt(1, "Exits the app.");
						}
						else {
							display.printAt(1, "Sorry, I could not find any command like that. Try something like 'help message'.");
						}
					}
				}
				else if (cmd == "host") { // host [password] <ipv6>

					if (splut.size() == 1) {
						display.printAt(0, "[@] Host:");
						display.printAt(1, "You need a password to host! (try 'help host')");
					}
					else {

						bool ipv6 = false;
						if (splut.size() > 2) ipv6 = ([](std::string s) { std::for_each(s.begin(), s.end(), std::tolower); return s; }(splut[2]) == "ipv6");

						std::string password = splut[1];

						display.printAt(0, "[@] Host " + std::string(ipv6 ? "IPV6" : "IPV4") + ":");
						display.printAt(1, "Startint host with password " + password + "...");

						// create host
						int line = 2;

						if (client) {
							display.printAt(line++, "You were as client. Cleaning up client...");
							delete client;
							client = nullptr;
						}

						if (host) {
							host->setMaxConnections(1);
							for (auto& i : *host) {
								i->kill();
							}				
						}
						else {
							host = Tools::new_guaranteed<Sockets::con_host>(ipv6);
							host->setMaxConnections(1);
						}

						input.clsInput();
						display.setCommandEntry("");

						display.printAt(line++, "Waiting for new connection... (type anything to stop and cancel or wait 120 sec)");

						Sockets::con_client* i = nullptr;

						for (size_t u = 0; u < 120 && !i; u++) {
							if (input.copyInput().length() > 0) {
								display.printAt(line++, "Got input, end...");
								host->setMaxConnections(0);
								for (auto& i : *host) i->kill();
								break;
							}
							i = host->waitNewConnection(1000);
						}

						auto assert_con = [&](bool f) {
							if (!f) {
								if (i) i->kill();
								input.clsInput();
								display.setCommandEntry("");
							}
							return f;
						};

						if (i) {
							display.printAt(line++, "Connected!");

							i->hookPrintBandwidth([&](std::string nm) { internal_message = nm; });
#ifdef LSW_DEBUG
							i->hookPrintEvent([&](std::string ev) {log_last(ev); });
#endif

							i->send(version_app, static_cast<int>(packages_id::VERSION_CHECK));

							std::shared_ptr<Sockets::final_package> pkg1, pkg2;

							display.printAt(line++, "Verifying version and password...");

							if (!assert_con(i->recv(pkg1, 2000))) continue; // should be version
							if (!assert_con(i->recv(pkg2, 2000))) continue; // should be password
							{
								bool failed = false;
								if (failed |= (pkg1->data_type != static_cast<int>(packages_id::VERSION_CHECK))) {
									display.printAt(line++, "The package ordering was not right... Version not found.");
								}
								if (failed |= (pkg2->data_type != static_cast<int>(packages_id::PASSWORD_CHECK))) {
									display.printAt(line++, "The package ordering was not right... Password could not be verified.");
								}
								if (failed) {
									assert_con(false);
									continue;
								}
							}

							// VERSION STRING

							if (pkg1->variable_data != version_app) {
								display.printAt(line++, "Version not supported!");
								i->kill();
								input.clsInput();
								display.setCommandEntry("");
								continue;
							}

							// STRING PASSWORD
							if (password.compare(0, password.size(), pkg2->variable_data, 4, password.size() + 4) == 0) {
								display.printAt(line++, "Password match!");
								i->send("OK", static_cast<int>(packages_id::PASSWORD_CHECK));
							}
							else {
								display.printAt(line++, "Password failed!");
								i->kill();
								input.clsInput();
								display.setCommandEntry("");
								continue;
							}

							if (thr_handle_packages) {
								display.printAt(line++, "Cleaning up old connection stuff first...");
								thr_handle_packages->join();
								delete thr_handle_packages;
							}
							thr_handle_packages = Tools::new_guaranteed<std::thread>([&, i]() {handlePackages(message_sent, transf_m, input.die, i, is_file_transfer_enabled, packages_coming, system_message); }); // if not connected, die.

							display.printAt(line++, "[!] Up and running!");
						}

						else {
							host->setMaxConnections(0);
							display.printAt(line++, "Time out or user input! Not accepting connections anymore.");
							assert_con(false);
						}

					}
				}
				else if (cmd == "connect") { // connect [ip] [password]

					if (splut.size() <= 2) {
						display.printAt(0, "[@] Connect:");
						display.printAt(1, "You need an IP and a password to connect! (try 'help connect')");
					}
					else {
						std::string ip = splut[1];
						std::string pw = splut[2];

						display.printAt(0, "[@] Connect:");
						display.printAt(1, "Trying to connect to " + ip + "...");

						// connect
						int line = 2;

						if (client) {
							display.printAt(line++, "Detected a connection already in memory, cleaning up...");
							client->kill();
							display.printAt(line++, "Ready, trying to connect...");
							delete client;
						}

						if (host) {
							display.printAt(line++, "You were as host. Cleaning up host...");
							delete host;
							host = nullptr;
						}

						client = Tools::new_guaranteed<Sockets::con_client>();
						client->hookPrintBandwidth([&](std::string nm) {internal_message = nm; });
#ifdef LSW_DEBUG
						client->hookPrintEvent([&](std::string ev) {log_last(ev); });
#endif

						auto assert_con = [&](bool f) {
							if (!f) {
								if (client) client->kill();
								input.clsInput();
								display.setCommandEntry("");
							}
							return f;
						};

						if (client->connect(ip.c_str())) {
							display.printAt(line++, "Connected! Verifying...");

							int rnd = GetTickCount64() % 1000 + 2000;

							client->send(version_app, static_cast<int>(packages_id::VERSION_CHECK));
							client->send(std::to_string(rnd) + pw, static_cast<int>(packages_id::PASSWORD_CHECK));

							std::shared_ptr<Sockets::final_package> pkg1, pkg2;

							if (!assert_con(client->recv(pkg1, 2000))) continue; // should be version
							if (!assert_con(client->recv(pkg2, 2000))) continue; // should be password
							{
								bool failed = false;
								if (failed |= (pkg1->data_type != static_cast<int>(packages_id::VERSION_CHECK))) {
									display.printAt(line++, "The package ordering was not right... Version not found.");
								}
								if (failed |= (pkg2->data_type != static_cast<int>(packages_id::PASSWORD_CHECK))) {
									display.printAt(line++, "The package ordering was not right... Password could not be verified.");
								}
								if (failed){
									assert_con(false);
									continue;
								}
							}

							// VERSION STRING

							if (pkg1->variable_data != version_app) {
								display.printAt(line++, "Version not supported!");
								client->kill();
								input.clsInput();
								display.setCommandEntry("");
								continue;
							}

							// STRING PASSWORD

							if (pkg2->variable_data == "OK") {
								display.printAt(line++, "Password match!");
							}
							else {
								display.printAt(line++, "Password failed!");
								client->kill();
								input.clsInput();
								display.setCommandEntry("");
								continue;
							}

							if (thr_handle_packages) {
								display.printAt(line++, "Cleaning up old connection stuff first...");
								thr_handle_packages->join();
								delete thr_handle_packages;
							}
							thr_handle_packages = Tools::new_guaranteed<std::thread>([&, client]() {handlePackages(message_sent, transf_m, input.die, client, is_file_transfer_enabled, packages_coming, system_message); }); // if not connected, die.

							display.printAt(line++, "[!] Up and running!");
						}
						else {
							display.printAt(line++, "Failed to connect!");
							assert_con(false);
						}
					}
				}
				else if (cmd == "add" || cmd == "a") {
					if (sending) {
						display.printAt(0, "[@] Add:");
						display.printAt(1, "Please wait for the transfer to end!");
					}
					else if (splut.size() == 1) {
						display.printAt(0, "[@] Add:");
						display.printAt(1, "If you want to add something, do 'add <something>'! (try 'help add')");
					}
					else {
						std::string toadd = splut[1];
						display.printAt(0, "[@] Add:");
						display.printAt(1, "Analysing file...");

						/*if (size_t sizz = toadd.length(); sizz > 1) {
							if (toadd[0] == '\"' && toadd[sizz - 1] == '\"') {
								toadd.pop_back();
								toadd = toadd.substr(1);
							}
						}*/

						auto size = Tools::getFileSize(toadd);

						if (size > 0) {
							bool canadd = true;
							for (auto& i : items_list) canadd &= (i.first != toadd);
							if (canadd) {
								items_list.push_back({ toadd, 0 });
								display.printAt(2, "Added file with " + std::to_string(size) + " bytes!");
							}
							else {
								display.printAt(2, "Item already in list!");
							}
						}
						else {
							display.printAt(2, "Invalid file!");
						}
					}
				}
				else if (cmd == "del" || cmd == "d") {
					if (sending) {
						display.printAt(0, "[@] Del:");
						display.printAt(1, "Please wait for the transfer to end!");
					}
					else if (splut.size() == 1) {
						display.printAt(0, "[@] Del:");
						display.printAt(1, "If you want to del something, do 'del <item number>'! (try 'help del')");
					}
					else {
						display.printAt(0, "[@] Del " + splut[1] + ":");

						size_t itemid;
						if (sscanf_s(splut[1].c_str(), "%zu", &itemid)) {
							if (itemid < items_list.size()) {
								std::string what_del = items_list[itemid].first;
								items_list.erase(items_list.begin() + itemid);
								display.printAt(1, "Removed: " + what_del);
							}
							else {
								display.printAt(1, "Out of range. Are you sure about this number?");
							}
						}
						else {
							display.printAt(1, "This number seems invalid. Please try again.");
						}
					}

				}
				else if (cmd == "show" || cmd == "s") { // show <page>					
					int limit = static_cast<int>(display.getLineAmount()) - 1;
					int pagemax = 1 + (static_cast<int>(items_list.size()) + 1) / limit;
					int from = 0;
					if (splut.size() > 1) {
						if (!sscanf_s(splut[1].c_str(), "%d", &from)) {
							from = 0;
						}
						else if (from > 0){
							from--;
						}
					}

					display.printAt(0, "[@] Show [" + std::to_string(from + 1) + "/" + std::to_string(pagemax) + "]:");

					for (int start = 0; start < limit; start++) {
						int itemid = start + from * limit;
						if (itemid < items_list.size()) {
							std::string cpyy = items_list[itemid].first;
							short progress = items_list[itemid].second;
							auto t = GetTickCount64();
							display.printAt(start + 1, [=]() {return shrinkText("#" + std::to_string(itemid) + (progress != 0 ? (" [" + std::to_string(progress) + "%]") : "") + ": " + cpyy, 100, 250, t); });
						}
					}
				}
				else if (cmd == "receivefiles" || cmd == "rf") {
					display.printAt(0, "[@] ReceiveFiles:");

					if (splut.size() > 1) {
						if (splut[1] == "disable") {
							is_file_transfer_enabled = false;
						}
						else if (splut[1] == "enable") {
							is_file_transfer_enabled = true;
						}
					}
					else is_file_transfer_enabled = !is_file_transfer_enabled;

					display.printAt(1, "Now you " + std::string((is_file_transfer_enabled ? "CAN" : "CANNOT")) + " receive new files.");
				}
				else if (cmd == "sendfiles" || cmd == "sf") {
					display.printAt(0, "[@] SendFiles:");

					int line = 1;

					if (sending) {
						display.printAt(line++, "Please wait the task to be done before doing it again!");
					}
					else {
						if (thr_autosend_packages) {
							display.printAt(line++, "Cleaning up last task...");
							thr_autosend_packages->join();
							delete thr_autosend_packages;
						}
						display.printAt(line++, "Initializing thread to send...");

						thr_autosend_packages = Tools::new_guaranteed<std::thread>([&]() {sendFileAuto(message_sent, transf_m, input.die, (host ? *host->begin() : client), items_list, sending, system_message); });

						display.printAt(line++, "Sending files!");
					}
					
				}
				else if (cmd == "message" || cmd == "m") {
					display.printAt(0, "[@] Message:");

					if ((host || client) && splut.size() > 1) {
						Sockets::con_client* the_client = client;
						if (host) {
							if (host->size() > 0) {
								the_client = *host->begin();
							}
						}

						if (!the_client)
						{
							display.printAt(1, "You are not connected!");
						}
						else if (!the_client->isConnected()) {
							display.printAt(1, "Sorry, but the connection got down!");
						}
						else {
							std::string to_send;
							for (size_t p = 1; p < splut.size(); p++) {
								to_send += splut[p] + " ";
							}
							if (to_send.length() > 0) to_send.pop_back();

							message_sent = false;
							the_client->send(to_send, static_cast<int>(packages_id::MESSAGE));
							Sockets::final_package pkg;
							display.printAt(1, "Sending message...");
							while (!message_sent) std::this_thread::sleep_for(std::chrono::milliseconds(70)); // whoever is running shall change message_sent to true if MESSAGE_SENT is got to ensure send

							if (GetTickCount64()/1000 < 20) display.printAt(2, "Voosh!");
							else display.printAt(2, "Sent!");

							display.newMessage("[You -> Them] " + to_send);
						}
					}
				}
				else if (cmd == "version") {
					display.printAt(0, "[@] Version:");
					display.printAt(1, version_app);
				}
				else if (cmd == "discord") {
					display.printAt(0, "[@] Discord:");
					display.printAt(1, discord_link);
				}
				else if (cmd == "tutorial" || cmd == "t") {
					display.printAt(0, "[@] Tutorial:");

					display.printAt(1, "You can host yourself by doing this:");
					display.printAt(2, "> Run 'host' with a password, like 97531, and then tell your friend the password;");
					display.printAt(3, "> They can connect to you by using 'connect <ip> <the password>';");
					display.printAt(4, "> Add items via 'add <path>';");
					display.printAt(5, "> Send them with 'sendfiles'. Your friend have to do 'receivefiles' to receive them.");
					display.printAt(6, "> Done.");

					display.printAt(8, "You can connect to your friend by doing this:");
					display.printAt(9, "> Run 'connect' with the IP and password they gave to you;");
					display.printAt(10, "> Accept their files with 'receivefiles';");
					display.printAt(11, "> At the end you have the files they wanted you to have! Enjoy.");

				}
				else if (cmd == "credits") {
					display.printAt(0, "[@] Credits:");

					display.printAt(1, "App developed by:");
					display.printAt(2, "- Lohkdesgds H. Lhuminury");

					display.printAt(4, "Icon created by:");
					display.printAt(5, "- Lupspie");

					display.printAt(7, "Special thanks to:");
					display.printAt(8, "- You, because you're using the app ;P");					
				}
				else if (cmd == "exit") {
					dead = true;
				}
				else if (cmd != "debug") {
				    display.printAt(0, "[@] 404 not found!");
				    display.printAt(1, "Sorry, this command was not found!");
				    display.printAt(2, "Need help? Try 'help' ;P");
				}

				if (cmd == "debug" || always_go_back_to_debug) {
					input.clsInput();
					display.setCommandEntry("");
					display.clearAllLines();


					for (bool keep = true; keep;) {
						while (input.copyInput().length() == 0) {
							handle_message();
							display.printAt(0, "[@] Debugging... (press E to exit, K to always go back or any other key to PAUSE)");

							Sockets::con_client* connected = client ? client : (host ? (host->size() > 0 ? (*host->begin()) : nullptr) : nullptr);
							int mode = connected ? (connected == client ? 2 : 1) : 0; // 2 client, 1 host, 0 none

							if (connected) {
								display.printAt(1, "Mode:              " + std::string(mode == 2 ? "CLIENT" : "HOSTING"));
								display.printAt(2, "Buffer SEND:       " + std::to_string(connected->hasSending()) + " packages");
								display.printAt(3, "Buffer RECV:       " + std::to_string(connected->hasPackage()) + " packages");
								display.printAt(4, "Bitrate SEND:      " + Tools::byteAutoString(static_cast<double>(connected->getSendTrafficPerSec()), 6, true) + "B/s");
								display.printAt(5, "Bitrate RECV:      " + Tools::byteAutoString(static_cast<double>(connected->getRecvTrafficPerSec()), 6, true) + "B/s");
								display.printAt(6, "Total data SEND:   " + Tools::byteAutoString(static_cast<double>(connected->getSendTrafficTotal()), 6, true) + "B");
								display.printAt(7, "Total data RECV:   " + Tools::byteAutoString(static_cast<double>(connected->getRecvTrafficTotal()), 6, true) + "B");
								display.printAt(8, "Total data sum:    " + Tools::byteAutoString(static_cast<double>(connected->getTotalTraffic()), 6, true) + "B");
								display.printAt(9, "Connection status: " + std::string(connected->isConnected() ? "healthy" : "lost"));
							}
							else {
								display.printAt(1, "Mode:              NONE/UNDEF");
								display.printAt(9, "Connection status: lost");
							}
						}
						if (input.copyInput() == "K") {
							always_go_back_to_debug = !always_go_back_to_debug;
							display.clearAllLines();
							display.printAt(0, "[@] Debug:");
							display.printAt(1, "Always go back to debug is now: " + std::string(always_go_back_to_debug ? "ENABLED" : "DISABLED"));
							if (always_go_back_to_debug) display.printAt(2, "You will have to enter 'E' everytime you want a new command.");
							display.printAt(3, "Updating this page in 4 seconds...");
							std::this_thread::sleep_for(std::chrono::seconds(4));
							input.clsInput();
						}
						else {
							if (input.copyInput() != "E") {
								input.clsInput();
								display.printAt(0, "[@] Debug PAUSED (press E to exit or any other key to PLAY)");
								while (input.copyInput().length() == 0) { handle_message(); std::this_thread::sleep_for(std::chrono::milliseconds(150)); }
								input.clsInput();
							}
							if (input.copyInput() == "E") // can come from != E
							{
								display.clearAllLines();
								display.printAt(0, "[@] You left the Debugging page.");
								keep = false;
							}
						}
					}
				}
			}


		}

		//Sleep(10);
		if (clear_input_waiting) {
			clear_input_waiting = false;
			input.clsInput();
		}
	}


	input.die = true;
	thr_input.join();
	thr_autorefresh.join();
	if (thr_handle_packages) {
		thr_handle_packages->join();
		delete thr_handle_packages;
	}
	if (thr_autosend_packages) {
		thr_autosend_packages->join();
		delete thr_autosend_packages;
	}

	return 0;
}



void input_handler(user_input& in) {
	while (!in.die) {
		int ch = Custom::getCH();

		in.m.lock();

		switch (ch) {
		case 27: // esc
			break;
		case 13: // enter
			in.hasbreak = true;
			break;
		case 8: // backspace
			if (in.input_going_on.length() > 0) in.input_going_on.pop_back();
			break;
		default:
			if (ch < 256) {
				in.input_going_on += ch;
			}
		}
		in.m.unlock();

		while (in.hasbreak && !in.die);
	}
}
void update_display(Custom::DISPLAY<100, 30>& d, bool& die) {
	size_t sometimes = 100;
	while (!die) {
		d.flip();
		if (sometimes++ > 100) {
			sometimes = 0;
			Custom::ShowConsoleCursor(false);
			Custom::disableEcho(true);
		}
		//std::this_thread::sleep_for(std::chrono::milliseconds(20));
	}
}
std::string shrinkText(std::string text, const size_t limit, const ULONGLONG ms, const ULONGLONG start, const size_t time_ticks_wait)
{
	int difference = static_cast<int>(text.length() - limit);

	if (difference > 0) {
		int movement = ((int)((GetTickCount64() - start) / ms) % (2 * (difference + time_ticks_wait)));
		int exc = movement > (difference + time_ticks_wait) ? (movement % (difference + time_ticks_wait)) : 0;

		if (movement > difference) movement = difference;
		if (movement < 0) movement = 0;

		movement -= exc;

		if (movement > difference) movement = difference;
		if (movement < 0) movement = 0;

		text = text.substr(movement);
		if (text.length() > limit) text = text.substr(0, limit);
	}

	return text;
}
void cleanAvoidPath(std::string& s)
{
	std::for_each(s.begin(), s.end(), [](char& c) {if (c == '/')  c = '$'; });
	std::for_each(s.begin(), s.end(), [](char& c) {if (c == '\\') c = '#'; });
	std::for_each(s.begin(), s.end(), [](char& c) {if (c == ':')  c = '-'; });
}


void sendFileAuto(bool& message_sent, SuperMutex& client_m, bool& die, Sockets::con_client* c, std::vector<std::pair<std::string, short>>& list, bool& working, std::string& syst) {
	auto giveup = [&](std::string err = "Could not proceed.") {working = false; syst = err; };
	if (!c) { giveup(); return; }
	if (!c->isConnected()) { giveup(); return; }


	const size_t default_time_to_update = 500;
	size_t packages_of_file = 0;

	auto getTime = [] {return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()); };
	auto aheadTime = [&](std::chrono::milliseconds m) {return getTime() - m; };
	auto latest_upd = getTime();

	auto timeToText = [&] {return (aheadTime(latest_upd).count() > default_time_to_update) ? ([&] {latest_upd = getTime(); return true; }()) : false; };

	auto msg = [&](const std::string s, const bool ignore_t = true) {if (!ignore_t && !timeToText()) return; syst = s; };
	auto perc = [&](const double d, const int dots = 0) {if (timeToText()) { char buff[32]; sprintf_s(buff, "P#%06zu|%02.1lf%c%s", packages_of_file, d, '%', [&]() {std::string u; for (int k = 0; k < 4; k++) if (k < dots) u += '.'; else u += ' '; return u; }().c_str()); msg("Sent " + std::string(buff)); }};



	//ULONGLONG latest_perc_upd = 0;
	//auto closeFileAndCleanUp = [/*&handle,*/ &syst, &filesiz, &readden]() {/*handle.close();*/ filesiz = 1;  readden = 0; };
	//auto msg = [&syst](const std::string s) {std::string c = std::to_string(GetTickCount64() % 10000); size_t a = c.length(); syst = "[@" + [&]() {std::string k; for (int u = 4; u > a; u--) k += '0'; return k; }() + c + "] " + s; };
	//auto perc = [&](const double d, const int dots = 0, std::string ext = "") {if (GetTickCount64() - latest_perc_upd > 200) { latest_perc_upd = GetTickCount64(); char buff[16]; sprintf_s(buff, "%02.1lf%c%s", d, '%', [&]() {std::string u; for (int k = 0; k < 4; k++) if (k < dots) u += '.'; else u += ' '; return u; }().c_str()); msg("Sent " + std::string(buff) + ext); }};

	msg("Checking if can actually send file...");

	client_m.lock();

	c->send("check", static_cast<int>(packages_id::FILE_AVAILABLE));

	std::shared_ptr<Sockets::final_package> pkg;
	if (!c->recv(pkg, 2000)) { client_m.unlock(); giveup("Timeout."); return; }

	if (pkg->data_type != static_cast<int>(packages_id::FILE_AVAILABLE)) { client_m.unlock(); giveup(); return; }
	if (pkg->variable_data != "yes") { client_m.unlock(); giveup("The other side doesn't accept file transfer."); return; }

	client_m.unlock();

	// sending

	working = true;

	// 1 skip/clean, 0 try again, -1 return
	auto handle_pack = [&](std::pair<std::string, short>& i)->int{

		SmartFILE handle;
		long long filesiz = Tools::getFileSize(i.first);
		long long readden = 0;

		if (!handle.open(i.first.c_str(), file_modes::READ)) {
			msg("File '" + i.first + "' was not found. Skipping...");
			std::this_thread::sleep_for(std::chrono::seconds(2));
			return 1;
		}

		client_m.lock();

		c->send(std::to_string(filesiz), static_cast<int>(packages_id::FILE_SIZE)); // FIRST!
		c->send(i.first, static_cast<int>(packages_id::FILE_OPEN));

		msg("Preparing to send " + Tools::byteAutoString(filesiz, 2) + "B...");

		std::shared_ptr<Sockets::final_package> pkg;
		do {
			if (!c->recv(pkg, 5000)) { 
				client_m.unlock();
				msg("Timeout on sync, trying again...");
				std::this_thread::sleep_for(std::chrono::seconds(2));
				return 0;
			}
			message_sent |= (pkg->data_type == static_cast<int>(packages_id::MESSAGE_SENT));
		} while (pkg->data_type == static_cast<int>(packages_id::MESSAGE_SENT));

		client_m.unlock();

		if (pkg->data_type != static_cast<int>(packages_id::FILE_OK)) {
			msg("Failed to sync file. Trying again...");
			std::this_thread::sleep_for(std::chrono::seconds(2));
			return 0;
		}

		packages_of_file = 0;
		//SmartFILE test;
		//test.open("THIS_IS_A_TEST.test", file_modes::WRITE);

		for (bool end = false; !end;) {

			if (c->isConnected())
			{
				std::shared_ptr<Sockets::final_package> pkg2 = std::make_shared<Sockets::final_package>();

				packages_of_file++;
				readden += handle.read(pkg2->variable_data, (Sockets::default_package_size << 1)); //  64 kB/tick

				if (handle.eof()) {
					if (readden != filesiz) {
						msg("File out of range?! FATAL ERROR");
						std::this_thread::sleep_for(std::chrono::seconds(10));
					}
					client_m.lock();
					end = true;
				}

				//test.write(pkg2->variable_data);

				pkg2->data_type = static_cast<int>(packages_id::FILE_RECEIVING);
				c->send(pkg2);
			}
			else {
				msg("Lost connection.");
				return -1;
			}

			perc(readden * 100.0 / filesiz, (getTime().count() / default_time_to_update) % 4);
		}
		//test.close();
		
		while (c->hasSending()) {
			msg("Waiting end of buffer" + [&](const int dots) {std::string u; for (int k = 0; k < 4; k++) if (k < dots) u += '.'; else u += ' '; return u; }((getTime().count() / default_time_to_update) % 4));
			std::this_thread::sleep_for(std::chrono::milliseconds(default_time_to_update));
		}

		std::this_thread::sleep_for(std::chrono::seconds(2));

		c->send("end", static_cast<int>(packages_id::FILE_CLOSE));
		msg("Waiting response... [#" + std::to_string(packages_of_file) + "]");

		if (c->recv(pkg, 30000)) {
			if (pkg->data_type != static_cast<int>(packages_id::FILE_OK)) {
				msg("Failed to sync file. Trying again (sorry)... [#" + std::to_string(packages_of_file) + "]");
				client_m.unlock();
				std::this_thread::sleep_for(std::chrono::seconds(5));
				return 0;
			}
			else {
				msg("Sent file successfully. Processing list... [#" + std::to_string(packages_of_file) + "]");
				client_m.unlock();
				std::this_thread::sleep_for(std::chrono::seconds(5));
			}
		}
		else {
			msg("Failed to sync file. Try again later :( [#" + std::to_string(packages_of_file) + "]");
			client_m.unlock();
			std::this_thread::sleep_for(std::chrono::seconds(5));
			return -1;
		}
		handle.close();

		return 1;
	};


	while (!die && list.size() > 0 && c->isConnected()) {

		// 1 skip/clean, 0 try again, -1 return
		int res = handle_pack(list[0]);

		switch (res) {
		case 1: // clean once
			list.erase(list.begin());
			break;
		case 0: // try again
			continue;
		case -1: // cancel all
			working = false;
			return;
		}
	}
	working = false;
	syst = "Ended all tasks.";
}

void handlePackages(bool& message_sent, SuperMutex& client_m, bool& die, Sockets::con_client* c, bool& receive, std::vector<Sockets::final_package>& messages, std::string& syst) {
	if (!c) return;
	if (!c->isConnected()) return;
	SmartFILE handle;
	LONGLONG filesiz = 1, written = 0;
	const size_t default_time_to_update = 500;
	size_t packages_of_file = 0;

	auto getTime = [] {return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()); };
	auto aheadTime = [&](std::chrono::milliseconds m) {return getTime() - m; };
	auto latest_upd = getTime();

	auto timeToText = [&](bool just_reset = false) {return (bool)(just_reset ? (1 + (latest_upd = getTime()).count()) : ((aheadTime(latest_upd).count() > default_time_to_update) ? ([&] {latest_upd = getTime(); return true; }()) : false)); };

	auto cleanUpFile = [&]() {filesiz = 1;  written = 0; packages_of_file = 0; };
	auto closeFileAndCleanUp = [&]() {handle.close(); cleanUpFile(); };
	auto msg = [&](const std::string s, const bool ignore_t = true) {if (!ignore_t && !timeToText()) return; timeToText(true); syst = s; };
	auto perc = [&](const double d, const int dots = 0) {if (timeToText()) { char buff[32]; sprintf_s(buff, "P#%06zu|%02.1lf%c%s", packages_of_file, d, '%', [&]() {std::string u; for (int k = 0; k < 4; k++) if (k < dots) u += '.'; else u += ' '; return u; }().c_str()); msg("Received " + std::string(buff)); }};
	
	//bool last_was_receive = false;

	msg("Waiting for event...");

	Sockets::final_package lmsg;

	//bool recvd = false;

	while (!die) {
		if (!c->isConnected()) {
			closeFileAndCleanUp();
			syst.clear();
			return;
		}
		std::shared_ptr<Sockets::final_package> pkg;

		//std::this_thread::sleep_for(std::chrono::milliseconds(10));

		client_m.lock();
		/*
		if (recvd ? true : (c->recv(pkg, 200))) { // recvd means it has already gotten a package (when FILE_RECEIVING is trigger, it loops until is it not, but then it did recv once)
			recvd = false;*/
		if (c->recv(pkg, 200)) {

			switch (pkg->data_type) {

			case static_cast<int>(packages_id::VERSION_CHECK):
			case static_cast<int>(packages_id::PASSWORD_CHECK):
				msg("Weird behaviour got! Bye.");
				c->kill();
				continue;
			case static_cast<int>(packages_id::MESSAGE):
				msg("Got message.");
				messages.push_back(*pkg);
				c->send("good", static_cast<int>(packages_id::MESSAGE_SENT));
				break;
			case static_cast<int>(packages_id::MESSAGE_SENT):
				message_sent = true;
				break;

			case static_cast<int>(packages_id::FILE_AVAILABLE):
				msg((receive ? "Possible file transfer incoming..." : "Blocked file transfer ('rf')"));
				c->send((receive ? "yes" : "no"), static_cast<int>(packages_id::FILE_AVAILABLE));
				break;

				// TRANSFER PART:

			case static_cast<int>(packages_id::FILE_SIZE):
				filesiz = atoll(pkg->variable_data.c_str());
				msg("Preparing to receive " + Tools::byteAutoString(filesiz, 2) + "B...");
				std::this_thread::sleep_for(std::chrono::seconds(1));
				break;

			case static_cast<int>(packages_id::FILE_OPEN):
				{
					handle.close();
					written = 0;

					cleanAvoidPath(pkg->variable_data);
					if (!handle.open(pkg->variable_data.c_str(), file_modes::WRITE)) {
						msg("Failed opening file to receive transfer!");
						c->send("notgood", static_cast<int>(packages_id::FILE_NOT_OK));
						client_m.unlock();
						continue;
					}
					else {
						msg("Opened file for transfer!");
						c->send("good", static_cast<int>(packages_id::FILE_OK));
					}
				}
				break;
			case static_cast<int>(packages_id::FILE_RECEIVING):
				{
					/*do {
						if (pkg->data_type != static_cast<int>(packages_id::FILE_RECEIVING)) {
							recvd = true;
							break;
						}*/

					packages_of_file++;
					written += handle.write(pkg->variable_data);
					if (filesiz > 0) perc(written * 100.0 / filesiz, (getTime().count() / default_time_to_update) % 4);
					else msg("Receiving " + std::to_string(written) + " byte(s)...", false);

					/*} while (c->recv(pkg, 2000));
					msg(recvd ? "Another package..." : "Timeout...", false);*/

					pkg->variable_data.clear();
				}
				break;
			case static_cast<int>(packages_id::FILE_CLOSE):
				if (written == filesiz) {
					msg("File saved successfully. [#" + std::to_string(packages_of_file) + "]");
					closeFileAndCleanUp();
					c->send("good", static_cast<int>(packages_id::FILE_OK));
				}
				else {
					msg("Something went wrong somewhere.");
					//closeFileAndCleanUp();
					c->send("notgood", static_cast<int>(packages_id::FILE_NOT_OK));
				}
				
				break;
			}
			//cleanUpFile(); // no close
		}
		/*else {
			msg("Waiting new event...");
		}*/
		client_m.unlock();
	}
	syst.clear();
}