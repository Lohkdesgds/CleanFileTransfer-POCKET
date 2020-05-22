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

#ifdef _DEBUG
#define IGNORE_TIMEOUT
#endif


using namespace LSW::v5;

const std::string version_app = "CleanFileTransfer POCKET beta 0.5b";
const std::string discord_link = std::string("ht") + std::string("tps://discord.gg/a5G") + "GgBt";

enum class packages_id {VERSION_CHECK=1, MESSAGE, PASSWORD_CHECK, FILE_AVAILABLE, FILE_RECEIVING, FILE_SIZE, FILE_OPEN, FILE_CLOSE};


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
void update_display(Custom::DISPLAY<100, 24>&, bool&);

// text, limit, time to bounce, specific time to set as init, how many ticks each end
std::string shrinkText(std::string, const size_t, const ULONGLONG, const ULONGLONG = 0, const size_t = 10);

// replaces "/" to "$", "\\" to "%" and ":" to "-"
void cleanAvoidPath(std::string&);

// Automatically sends a file through the con_client. Mutex guarantee package read, Bool is for "kill" if needed, second bool is to tell you if it is working on something
void sendFileAuto(SuperMutex&, bool&, Sockets::con_client*, std::vector<std::pair<std::string, short>>&, bool&, std::string&);
// Handles packages. It will save messages on the vector. Mutex guarantee package read, First bool is "kill". Files are saved if second bool is true, else discarded. It dies automatically if connection dies too. Last std::string is for messages from "system"
void handlePackages(SuperMutex&, bool&, Sockets::con_client*, bool&, std::vector<Sockets::final_package>&, std::string&);


int main() {
	std::cout << "Initializing..." << std::endl;

	Custom::ShowConsoleCursor(false);
	Custom::disableEcho(true);

	user_input input;
	Custom::DISPLAY<100, 24> display;
	Sockets::con_host* host = nullptr;
	Sockets::con_client* client = nullptr;
	std::vector<std::pair<std::string,short>> items_list;
	bool sending = false;
	std::vector<Sockets::final_package> packages_coming;
	bool is_file_transfer_enabled = false;
	std::string system_message;
	std::string internal_message;
	SuperMutex transf_m;


	std::thread thr_input([&]() { input_handler(input); });
	std::thread thr_autorefresh([&]() { update_display(display, input.die); });
	std::thread* thr_handle_packages = nullptr; // exists if there's a connection
	std::thread* thr_autosend_packages = nullptr; // exists while has something to send

	//thr_handle_packages = new std::thread([&]() {handlePackages(transf_m, input.die, (host ? *host->begin() : client), is_file_transfer_enabled, packages_coming, system_message); }); // if not connected, die.
	//thr_autosend_packages = new std::thread([&]() {sendFileAuto(transf_m, input.die, (host ? *host->begin() : client), items_list, sending, system_message); });



	display.setTitle([&]() {
		std::string buf = version_app + " | L: " + (system_message.length() > 0 ? system_message : "none") + " I: " + (internal_message.length() > 0 ? internal_message : "none") + " ";
		if (host) {
			size_t connected = 0;
			for (auto& i : *host) {
				connected += i->still_on();
			}
			buf += "| C/T: " + std::to_string(connected) + "/" + std::to_string(host->size());
		}
		if (client) {
			buf += "| C ";
			if (client->still_on()) buf += "ON";
			else buf += "OFF";
		}
		buf += " | Items: " + std::to_string(items_list.size());

		return shrinkText(buf, 100, 200);
		});

	display.printAt(0, "[@] Hello! Start with 'help' if you don't have any idea how this app works.");
		
	bool clear_input_waiting = false;
	std::string last_input = "";

	for (bool dead = false; !dead;) {
		Sleep(150);

		/*if (host || client) {
			Sockets::con_client* i = client;
			if (host) {
				if (host->size() > 0) {
					i = *host->begin();
				}
			}

			if (i) {
				if (i->hasPackage()) {
					Sockets::final_package pkg;
					i->recv(pkg);
					if (pkg.data_type == static_cast<int>(packages_id::MESSAGE)) {
						display.newMessage("[Them -> You] " + pkg.variable_data);
					}
					else packages_coming.push_back(pkg);
				}
			}
		}*/

		if (packages_coming.size() > 0) {
			Sockets::final_package& pkg = packages_coming[0];
			if (pkg.data_type == static_cast<int>(packages_id::MESSAGE)) {
				display.newMessage("[Them -> You] " + std::string(pkg.variable_data.data()));
			}
			packages_coming.erase(packages_coming.begin());
		}

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
								i->kill_connection();
							}				
						}
						else {
							host = new Sockets::con_host(ipv6);
							host->setMaxConnections(1);
						}

						bool cancel = false;

						input.clsInput();
						display.setCommandEntry("");

						display.printAt(line++, "Waiting for new connection... (type anything to stop and cancel or wait 120 sec)");

						auto timeout = GetTickCount64() + 120e3; // 60 sec
						auto timedout_f = [&]()->bool {return (cancel |= ((GetTickCount64() > timeout) || (input.copyInput().length() != 0))); };

						while (host->size() == 0 && !timedout_f()) Sleep(50);							

						if (!cancel) {
							auto& i = *host->begin();
							i->hookPrint([&](std::string nm) { internal_message = nm; });

							i->send({ version_app, static_cast<int>(packages_id::VERSION_CHECK) });

							Sockets::final_package pkg1, pkg2;

							while (!i->recv(pkg1) && i->still_on()) Sleep(20); // should be version
							while (!i->recv(pkg2) && i->still_on()) Sleep(20); // should be password
							{
								bool failed = false;
								if (failed |= (pkg1.data_type != static_cast<int>(packages_id::VERSION_CHECK))) {
									display.printAt(line++, "The package ordering was not right... Version not found.");
								}
								if (failed |= (pkg2.data_type != static_cast<int>(packages_id::PASSWORD_CHECK))) {
									display.printAt(line++, "The package ordering was not right... Password could not be verified.");
								}
								if (failed) {
									i->kill_connection();
									input.clsInput();
									display.setCommandEntry("");
									continue;
								}
							}

							// VERSION STRING

							if (pkg1.variable_data != version_app) {
								display.printAt(line++, "Version not supported!");
								i->kill_connection();
								input.clsInput();
								display.setCommandEntry("");
								continue;
							}

							pkg2.variable_data.erase(0, 4);

							// STRING PASSWORD
							if (memcmp(password.data(), (pkg2.variable_data.data()), password.length()) == 0) {
								display.printAt(line++, "Password match!");
								i->send({ "OK", static_cast<int>(packages_id::PASSWORD_CHECK) });
							}
							else {
								display.printAt(line++, "Password failed!");
								i->kill_connection();
								input.clsInput();
								display.setCommandEntry("");
								continue;
							}

							if (thr_handle_packages) {
								display.printAt(line++, "Cleaning up old connection stuff first...");
								thr_handle_packages->join();
								delete thr_handle_packages;
							}
							thr_handle_packages = new std::thread([&, i]() {handlePackages(transf_m, input.die, i, is_file_transfer_enabled, packages_coming, system_message); }); // if not connected, die.

							display.printAt(line++, "[!] Up and running!");
						}

						if (cancel) {
							host->setMaxConnections(0);
							display.printAt(line++, "Time out or user input! Not accepting connections anymore.");
							input.clsInput();
							display.setCommandEntry("");
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
							client->kill_connection();
							display.printAt(line++, "Ready, trying to connect...");
							delete client;
						}

						if (host) {
							display.printAt(line++, "You were as host. Cleaning up host...");
							delete host;
							host = nullptr;
						}

						client = new Sockets::con_client();
						client->hookPrint([&](std::string nm) {internal_message = nm; });

						auto timeout = GetTickCount64() + 10e3; // 60 sec
						auto timedout_f = [&timeout]()->bool {return GetTickCount64() > timeout; };

						if (client->connect(ip.c_str())) {
							display.printAt(line++, "Connected! Verifying...");

							int rnd = GetTickCount64() % 1000 + 2000;

							client->send({ version_app, static_cast<int>(packages_id::VERSION_CHECK) });
							client->send({ std::to_string(rnd) + pw, 3 });

							Sockets::final_package pkg1, pkg2;

							while (!client->recv(pkg1) && client->still_on()) Sleep(20); // should be version
							while (!client->recv(pkg2) && client->still_on()) Sleep(20); // should be password
							{
								bool failed = false;
								if (failed |= (pkg1.data_type != static_cast<int>(packages_id::VERSION_CHECK))) {
									display.printAt(line++, "The package ordering was not right... Version not found.");
								}
								if (failed |= (pkg2.data_type != static_cast<int>(packages_id::PASSWORD_CHECK))) {
									display.printAt(line++, "The package ordering was not right... Password could not be verified.");
								}
								if (failed){
									client->kill_connection();
									input.clsInput();
									display.setCommandEntry("");
									continue;
								}
							}

							// VERSION STRING

							if (pkg1.variable_data != version_app) {
								display.printAt(line++, "Version not supported!");
								client->kill_connection();
								input.clsInput();
								display.setCommandEntry("");
								continue;
							}

							// STRING PASSWORD

							if (pkg2.variable_data == "OK") {
								display.printAt(line++, "Password match!");
							}
							else {
								display.printAt(line++, "Password failed!");
								client->kill_connection();
								input.clsInput();
								display.setCommandEntry("");
								continue;
							}

							if (thr_handle_packages) {
								display.printAt(line++, "Cleaning up old connection stuff first...");
								thr_handle_packages->join();
								delete thr_handle_packages;
							}
							thr_handle_packages = new std::thread([&, client]() {handlePackages(transf_m, input.die, client, is_file_transfer_enabled, packages_coming, system_message); }); // if not connected, die.

							display.printAt(line++, "[!] Up and running!");
						}
						else {
							display.printAt(line++, "Failed to connect!");
							client->kill_connection();
							input.clsInput();
							display.setCommandEntry("");
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

						thr_autosend_packages = new std::thread([&]() {sendFileAuto(transf_m, input.die, (host ? *host->begin() : client), items_list, sending, system_message); });

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
						else if (!the_client->still_on()) {
							display.printAt(1, "Sorry, but the connection got down!");
						}
						else {
							std::string to_send;
							for (size_t p = 1; p < splut.size(); p++) {
								to_send += splut[p] + " ";
							}
							if (to_send.length() > 0) to_send.pop_back();

							the_client->send(Sockets::final_package{ to_send, 2 });

							if (GetTickCount64()/1000 < 20) display.printAt(1, "Voosh!");
							else display.printAt(1, "Sent!");

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
				else {
				    display.printAt(0, "[@] 404 not found!");
				    display.printAt(1, "Sorry, this command was not found!");
				    display.printAt(2, "Need help? Try 'help' ;P");
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
void update_display(Custom::DISPLAY<100, 24>& d, bool& die) {
	while (!die) {
		d.flip();
		Sleep(20);
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


void sendFileAuto(SuperMutex& client_m, bool& die, Sockets::con_client* c, std::vector<std::pair<std::string, short>>& list, bool& working, std::string& syst) {
	auto giveup = [&](std::string err = "Could not proceed.") {working = false; syst = err; c->considerEmptyPackagesOnDataFlow(true); };
	if (!c) { giveup(); return; }
	if (!c->still_on()) { giveup(); return; }
	FILE* handle = nullptr;
	LONGLONG filesiz = 1, readden = 0;
	ULONGLONG latest_perc_upd = 0;
	auto closeall = [&handle, &syst, &filesiz, &readden]() {if (handle) fclose(handle); filesiz = 1;  readden = 0; };
	auto msg = [&syst](const std::string s) {std::string c = std::to_string(GetTickCount64() % 10000); size_t a = c.length(); syst = "[@" + [&]() {std::string k; for (int u = 4; u > a; u--) k += '0'; return k; }() + c + "] " + s; };
	auto perc = [&](const double d, const int dots = 0, std::string ext = "") {if (GetTickCount64() - latest_perc_upd > 200) { latest_perc_upd = GetTickCount64();  char buff[16]; sprintf_s(buff, "%02.1lf%c%s", d, '%', [&]() {std::string u; for (int k = 0; k < 4; k++) if (k < dots) u += '.'; else u += ' '; return u; }().c_str()); msg("Sent " + std::string(buff) + ext); }};

	msg("Checking if can actually send file...");

	{
		//AutoLock al(client_m);
		client_m.lock();
		// can receive?
		//while (c->hasPackage()) { Sockets::final_package p; c->recv(p); } // yeah I know, but this guarantee no mess up ;-;

		c->send({ "check", static_cast<int>(packages_id::FILE_AVAILABLE) });

		while (!c->hasPackage() && c->still_on()) Sleep(20); // wait confirmation
		{
			Sockets::final_package pkg;
			c->recv(pkg);

			if (pkg.data_type != static_cast<int>(packages_id::FILE_AVAILABLE)) { client_m.unlock(); giveup(); return; }
			if (pkg.variable_data != "yes") { client_m.unlock(); giveup("The other side doesn't accept file transfer."); return; }
		}
	}
	client_m.unlock();

	c->considerEmptyPackagesOnDataFlow(false);

	// sending

	working = true;

	while (!die && list.size() > 0 && c->still_on()) {
		auto& i = list[0];

		filesiz = Tools::getFileSize(i.first);
		auto err = lsw_fopen(handle, i.first.c_str(), "rb");

		if (filesiz <= 0 || err) {
			msg("File '" + i.first + "' was not found. Skipping...");
			list.erase(list.begin());
			Sleep(2000);
			continue;
		}

		double buf_proc = 0.0f;

		msg("Waiting connection to sync...");

		{
			client_m.lock();

			c->send({ std::to_string(filesiz), static_cast<int>(packages_id::FILE_SIZE) }); // FIRST!
			c->send({ i.first, static_cast<int>(packages_id::FILE_OPEN) });

			while (!c->hasPackage() && c->still_on()) Sleep(20); // wait confirmation

			Sockets::final_package pkg;
			c->recv(pkg);

			client_m.unlock();

			if (pkg.data_type != static_cast<int>(packages_id::FILE_OPEN)) {
				msg("Failed to sync file. Trying again...");
				Sleep(2000);
				continue;
			}

		}

		c->setSpeed(Sockets::internet_way::UPLOAD, 0);

		size_t once_buf_full = 0;

		for (bool end = false; !end;) {

			/*if (c->hasPackage()) {
				Sockets::final_package pkg;
				c->recv(pkg);
				if (pkg.data_type == static_cast<int>(packages_id::FILE_CLOSE))
				{
					msg("Lost connection.");
					end = true;
					if (handle) fclose(handle);
					handle = nullptr;
					continue;
				}
			}*/

			auto sending_pending = c->hasSending();

			if (sending_pending < 40)
			{
				string_sized buf;
				readden += lsw_fread(handle, buf, end, (Sockets::default_package_size << 1)); //  16 kB/tick
				//readden += Sockets::default_package_size; // TEST
				//c->send({ buf, static_cast<int>(packages_id::FILE_RECEIVING) }); // maybe it's null
				c->send({ buf.data(), buf.size(), static_cast<int>(packages_id::FILE_RECEIVING) }); // maybe it's null
				if (once_buf_full) once_buf_full--;
			}
			else {
				ULONGLONG timeoutt = GetTickCount64() + static_cast<ULONGLONG>(10e3);
				while (c->hasSending() >= 10 && !end) {
					if (GetTickCount64() > timeoutt) {
						if (once_buf_full < 10) once_buf_full++;
#ifndef IGNORE_TIMEOUT
						msg("Lost connection.");
						end = true;
						if (handle) fclose(handle);
						handle = nullptr;
						continue;
#else
						msg("Lost connection? Keeping it up.");
#endif
					}
				}
			}

			buf_proc = ((buf_proc * 29.0) + 1.0 * (sending_pending * 100 / 10)) / 30.0;

			perc(readden * 100.0 / filesiz, (GetTickCount64() / 300) % 4, ", b=" + std::to_string(static_cast<int>(once_buf_full > 0 ? 101 : buf_proc)) + "% f=" + std::to_string(list.size()));
		}

		c->setSpeed(Sockets::internet_way::UPLOAD);

		if (handle) {
			msg("Sent file successfully.");
			c->send({ "end", static_cast<int>(packages_id::FILE_CLOSE) });
			fclose(handle);
		}

		list.erase(list.begin());
	}
	c->considerEmptyPackagesOnDataFlow(true);
	working = false;
	syst.clear();
}

void handlePackages(SuperMutex& client_m, bool& die, Sockets::con_client* c, bool& receive, std::vector<Sockets::final_package>& messages, std::string& syst) {
	if (!c) return;
	if (!c->still_on()) return;
	FILE* handle = nullptr;
	LONGLONG filesiz = 1, written = 0;
	ULONGLONG latest_perc_upd = 0;

	auto closeall = [&handle, &syst, &filesiz, &written]() {if (handle) fclose(handle); filesiz = 1;  written = 0; };
	auto msg = [&syst](const std::string s) {std::string c = std::to_string(GetTickCount64() % 10000); size_t a = c.length(); syst = "[#" + [&]() {std::string k; for (int u = 4; u > a; u--) k += '0'; return k; }() + c + "] " + s; };
	auto perc = [&](const double d) {if (GetTickCount64() - latest_perc_upd > 200) { latest_perc_upd = GetTickCount64();  char buff[8]; sprintf_s(buff, "%02.1lf%c", (d > 100.0 ? 100.0 : (d < 0.0 ? 0.0 : d)), '%'); msg("Received " + std::string(buff)); }};
	
	//bool last_was_receive = false;

	msg("Waiting for event...");

	while (!die) {
		if (!c->still_on()) {
			closeall();
			syst.clear();
			return;
		}

		Sockets::final_package pkg;

		//std::this_thread::sleep_for(std::chrono::milliseconds(10));

		client_m.lock();

		if (c->recv(pkg)) {

			switch (pkg.data_type) {

			case static_cast<int>(packages_id::MESSAGE):
				msg("Got message.");
				messages.push_back(pkg);
				break;

			case static_cast<int>(packages_id::FILE_AVAILABLE):
				msg((receive ? "Possible file transfer incoming..." : "Blocked file transfer ('rf')"));
				c->send({(receive ? "yes" : "no"), static_cast<int>(packages_id::FILE_AVAILABLE)});
				break;

				// TRANSFER PART:

			case static_cast<int>(packages_id::FILE_SIZE):
				filesiz = 0;
				filesiz = atoll(pkg.variable_data.c_str());
				break;

			case static_cast<int>(packages_id::FILE_OPEN):
				{
					if (handle) fclose(handle);

					cleanAvoidPath(pkg.variable_data);
					auto err = lsw_fopen(handle, pkg.variable_data.c_str(), "wb");
					if (err || !handle) {
						msg("Failed opening file to receive transfer!");
						c->send({ "notgood", static_cast<int>(packages_id::FILE_CLOSE) });
						handle = nullptr;
						client_m.unlock();
						c->considerEmptyPackagesOnDataFlow(true);
						c->setSpeed(Sockets::internet_way::DOWNLOAD);
						continue;
					}
					else {
						msg("Opened file for transfer!");
						c->considerEmptyPackagesOnDataFlow(false);
						c->setSpeed(Sockets::internet_way::DOWNLOAD, 0);
						c->send({ "good", static_cast<int>(packages_id::FILE_OPEN) });
					}
				}
				break;

			/*case static_cast<int>(packages_id::FILE_SIZE) : // HAS TO BE FIRST
				filesiz = 0;
				filesiz = atoll(pkg.variable_data.c_str());
				if (filesiz > 0) {
					c->considerEmptyPackagesOnDataFlow(false);

					msg("File size to transfer received: " + std::to_string(filesiz) + " bytes.");

					for (ULONGLONG t = GetTickCount64() + static_cast<ULONGLONG>(10e3); GetTickCount64() < t && !c->hasPackage();) Sleep(10);

					if (!c->hasPackage()) { // discard file size, took too long
#ifndef IGNORE_TIMEOUT
						c->send({ "timeout", static_cast<int>(packages_id::FILE_CLOSE) });
						msg("Failed to receive file. Reason: Timeout");
						client_m.unlock();
						c->considerEmptyPackagesOnDataFlow(true);
						continue;
#else
						msg("Timeout? Keeping up.");
						while (!c->hasPackage() && c->still_on()) Sleep(20);
#endif
					}
					c->recv(pkg);

					if (pkg.data_type != static_cast<int>(packages_id::FILE_OPEN)) {
						c->send({ "wrong", static_cast<int>(packages_id::FILE_CLOSE) });
						msg("Failed to receive file. Reason: Corrupted data");
						client_m.unlock();
						c->considerEmptyPackagesOnDataFlow(true);
						continue;
					}

					if (handle) fclose(handle);


					cleanAvoidPath(pkg.variable_data);
					auto err = lsw_fopen(handle, pkg.variable_data.c_str(), "wb");
					if (err || !handle) {
						msg("Failed opening file to receive transfer!");
						c->send({ "noopen", static_cast<int>(packages_id::FILE_CLOSE) });
						handle = nullptr;
						client_m.unlock();
						c->considerEmptyPackagesOnDataFlow(true);
						continue;
					}

					msg("Opened file for transfer!");

					c->send({ "good", static_cast<int>(packages_id::FILE_OPEN) });

					msg("Ready to receive data!");

					bool end_success = true;

					c->setSpeed(Sockets::internet_way::DOWNLOAD, 0);
					ULONGLONG t = GetTickCount64() + static_cast<ULONGLONG>(10e3);
					pkg = Sockets::final_package();

					do {
						if (GetTickCount64() > t) {
#ifndef IGNORE_TIMEOUT
							pkg.data_type = static_cast<int>(packages_id::FILE_CLOSE);
							end_success = false;
							c->send({ "timeout", static_cast<int>(packages_id::FILE_CLOSE) });
							msg("Failed to receive file. Reason: Timeout");
							c->considerEmptyPackagesOnDataFlow(true);
							break;
#else
							msg("This is taking some time... P=" + std::to_string(pkg.data_type) + ";S=" + std::to_string(pkg.variable_data.size()));
#endif
						}
							
						if (c->recv(pkg)) {
							//msg("Has package!");
							if (pkg.data_type == static_cast<int>(packages_id::FILE_RECEIVING)) {
								written += lsw_fwrite(handle, pkg.variable_data);
								//written += Sockets::default_package_size; // TEST
								if (filesiz) perc(written * 100.0 / filesiz);
								else msg("Receiving " + std::to_string(written) + " byte(s)...");
								t = GetTickCount64() + static_cast<ULONGLONG>(10e3);
							}
						}
					} while (pkg.data_type != static_cast<int>(packages_id::FILE_CLOSE));

					c->setSpeed(Sockets::internet_way::DOWNLOAD);

					closeall();
					c->considerEmptyPackagesOnDataFlow(true);
				}
				else filesiz = 1;
				break;*/
			case static_cast<int>(packages_id::FILE_RECEIVING):
				{
					written += lsw_fwrite(handle, pkg.variable_data);
					if (filesiz > 0) perc(written * 100.0 / filesiz);
					else msg("Receiving " + std::to_string(written) + " byte(s)...");
					//t = GetTickCount64() + static_cast<ULONGLONG>(10e3);
				}
				break;
			case static_cast<int>(packages_id::FILE_CLOSE):
				closeall();
				msg("File saved successfully.");
				c->setSpeed(Sockets::internet_way::DOWNLOAD);
				c->considerEmptyPackagesOnDataFlow(true);
				break;
			}
		}
		client_m.unlock();
		//if (!last_was_receive) Sleep(20);
	}
	syst.clear();
}