#define _WIN32_WINNT 0x0500

#include <iostream>
#include <stdlib.h>
#include <stdio.h>
#include <string>
#include <mutex>
#include <thread>
#include <vector>
#include <algorithm>

#include <LSWv5.h>

#include "display_cmd.h"
#include "multiline_console.h"
#include "channel_control.h"
#include "transf_safe.h"
//#include "tempbuffering.h"

constexpr int WIDTH = 101;
constexpr int HEIGHT = 30;

constexpr size_t max_name_length = 30; // also channel
constexpr size_t max_input_size = 500;
constexpr size_t max_file_name = max_input_size;

//constexpr size_t max_buffer = 20;
//constexpr size_t package_size_each = (/*20 * */Interface::connection::maximum_package_size);

constexpr size_t items_per_list = 5;
constexpr size_t title_client_len = 20;

//constexpr size_t packages_until_signal = 3;
//constexpr size_t high_performance_cooldown = 200;

using namespace LSW::v5;

const std::string version_check = "V1.0.0";
const std::string version_app = "CleanFileTransfer Pocket " + version_check + " LSW edition";
const std::string default_naming_top = "CFT - ";
const std::string discord_link = std::string("ht") + std::string("tps://discord.gg/Jkz") + "JjCG";
const std::string default_connection_url = "blackmotion.dynv6.net";

const std::string communication_version = "V1.3.2";

const std::string local_prefix = "[L] ";
const std::string server_prefix = "[S] ";

const std::string empty_line (WIDTH-1, ' ');

/*
Sending | Receiving

FILE_ASK_SEND >>
<< FILE_ACCEPT_REQUEST_SEND
FILE_TRANSFER >>
FILE_TRANSFER_CLOSE >>
*/


enum class packages_id {
	VERSION_CHECK=1,
	SERVER_MESSAGE,					// When a user login, or something
	MESSAGE,						// New message. Display and return MESSAGE_RECEIVED to host [ON MESSAGE MODE]
	MESSAGE_RECEIVED,				// The other one received. Display what you sent.			[ON MESSAGE MODE]
	REQUEST_TYPING,					// request server typing amount
	TYPING,							// on key stroke, send typing event
	//SIGNAL,						// after some packages, SIGNAL will go through the server and back to self
	SET_CHANNEL,					// Set user channel
	SET_NICK,						// Set nickname
	FILE_ASK_SEND,					// Who wants to send send this. Who doesn't, receive file_send_request with ID
	FILE_ACCEPT_REQUEST_SEND,		// Send back file_send_request
	LIST_REQUEST,					// Asks server for list on current queue
	FILE_TRANSFER,					// Sending data
	FILE_TRANSFER_CANCEL_NO_USER,   // Server sends this if no user is downloading files
	FILE_TRANSFER_CLOSE				// Close file, done (SHOULD NOT HAVE DATA WITH IT)
};

struct status {
	bool quitting = false;
	bool on_transfer = false;
	size_t packages_transf = 0;
	size_t packages_started_on = 0; // ref, there can be data already on going
};

struct file_send_request {
	char from[max_name_length + 1] = { 0 };
	char filename[max_file_name + 1] = { 0 };
	int64_t filesize = 0;
	uintptr_t _origin_transfer = 0; // set when server redirects
};


std::string compress(const packages_id, const std::string&);
// superthread's thing, connection now, file (opened already please), file's mutex, packages counter++, /*signal,*/ perc, copy of file request, on trasf changer
void send_thread(Tools::boolThreadF, Interface::Connection&, Interface::SmartFile&, Tools::SuperMutex&, size_t&,/* bool&,*/ double&, file_send_request, bool&);

int as_host();

int main(int argc, char* argv[]) {

	// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //
	// * * * * * * * * * * * * * * * * * * * / " " " " " " " " " " " " " " " " " " " " " " " " " " " " \ * * * * * * * * * * * * * * * * * * * //
	// * * * * * * * * * * * * * * * * * * * |                      STARTING CODE                      | * * * * * * * * * * * * * * * * * * * // 
	// * * * * * * * * * * * * * * * * * * * \ . . . . . . . . . . . . . . . . . . . . . . . . . . . . / * * * * * * * * * * * * * * * * * * * //
	// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

	std::string url = default_connection_url;
	std::string current_channel = "PUBLIC", current_user = "";
	size_t this_packages_total_count = 0;
	long long last_typing_amount = 0;
	//size_t high_performance_needed = high_performance_cooldown;
	//bool signal_got = false;
	std::chrono::seconds last_typing_sent = std::chrono::seconds(0);

	double file_transf_perc = 1.0;

	if (argc > 1) {
		if (strcmp(argv[1], "--server") == 0) return as_host();
		else url = argv[1];
	}

	// The connection, untouchable
	Interface::Connection conn;
	// The display, untouchable too
	std::shared_ptr<Custom::CmdDisplay<WIDTH, HEIGHT>> display = std::make_shared<Custom::CmdDisplay<WIDTH, HEIGHT>>();

	// User input, may change
	Custom::UserInput userinput;

	// Status, reference
	status status;

	Custom::MultiLiner liner;

	Tools::SuperThread<> file_send_thr{Tools::superthread::performance_mode::PERFORMANCE};
	Tools::SuperMutex file_m;
	Interface::SmartFile file;
	
	file_send_request last_request;	

	//conn.set_max_buffering(max_buffer);

	display->set_window_name(version_app);

	conn.debug_error_function([&](const std::string& err) {liner << "&c" + local_prefix + "Something bad happened: " + err; });
	
	// border function
	display->add_drawing_func([](int x, int y, Tools::char_c& ch)->bool {
		if (x == -1) return true;

		if ((x % 2 == 0) && (y == 0 || y == 2 || y == (HEIGHT - 3) || y == (HEIGHT - 1))) ch = { '*', Tools::cstring::C::DARK_GRAY };
		else if (x == 0 || x == (WIDTH - 1)) ch = { '*', Tools::cstring::C::DARK_GRAY };
		else if (x == 2 && y == (HEIGHT - 2)) ch = { '>', Tools::cstring::C::BLUE };

		return false;
	});

	// Title update
	display->add_tick_func([&](Custom::CmdDisplay<WIDTH, HEIGHT>& thus) {

		unsigned doublesecs = ((std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count() / 333) % 20);
		bool on = !(doublesecs % 2 == 1 && doublesecs < 8);

		std::string transl_title;

		if (status.quitting) {
			transl_title = "Exiting the app...";
		}
		else if (status.on_transfer) {
			transl_title = "Transfering data...";
		}
		else if (conn.is_connected()) {
			transl_title = "Connected";
		}
		else {
			transl_title = "Standby";
		}

		thus.draw_at(1, 2,
			Tools::sprintf_a("&f%-*s &8| &3%c &a%sB &b%.3lf%c &8| &3^&a%sB &3v&a%sB &8| &3W^%03.0lf%% &7%s %s",
				title_client_len, transl_title.substr(0, title_client_len).c_str(),
				/*max_name_length, (current_channel.empty() ? "PUBLIC" : current_channel).c_str(),*/
				file.is_open() ? ((file_m.is_locked()) ? 'L' : '%') : '.',
				Tools::byte_auto_string(this_packages_total_count).c_str(),
				(file_transf_perc > 1.0 ? 1.0 : file_transf_perc) * 100.0, '%',
				Tools::byte_auto_string(conn.get_network_info().send_get_total()).c_str(),
				Tools::byte_auto_string(conn.get_network_info().recv_get_total()).c_str(),
				(conn.buffer_sending_load() * 100.0),
				(last_typing_amount ? (std::to_string(last_typing_amount) + " typing...") : "").c_str(),
				empty_line.c_str()
			), false);

	});

	// set already feedback
	userinput.on_keystroke([&, display](const std::string& str) {

		//const auto max_len = WIDTH - 6;
		//display->draw_at((HEIGHT - 2), 4, str.substr(str.length() > max_len ? str.length() - max_len : 0, max_len) + empty_line);

		display->draw_at((HEIGHT - 2), 4, str + ' ');

		if (conn.is_connected() && std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()) - last_typing_sent >= (Custom::time_typing_limit - std::chrono::seconds(1))) {
			last_typing_sent = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch());
			conn.send_package(compress(packages_id::TYPING, ""));
		}
	});
	userinput.limit_max_length(max_input_size);
	

	liner.set_line_amount(HEIGHT - 7);
	liner.set_line_width_max(WIDTH - 2);
	liner.set_func_draw([&,display](int py, const Tools::Cstring& str) {
		//display->draw_at(py + 3, 1, empty_line);
		display->draw_at(py + 3, 1, str);
	});

	// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //
	// * * * * * * * * * * * * * * * * * * * / " " " " " " " " " " " " " " " " " " " " " " " " " " " " \ * * * * * * * * * * * * * * * * * * * //
	// * * * * * * * * * * * * * * * * * * * |                  CONNECTING TO SERVER                   | * * * * * * * * * * * * * * * * * * * // 
	// * * * * * * * * * * * * * * * * * * * \ . . . . . . . . . . . . . . . . . . . . . . . . . . . . / * * * * * * * * * * * * * * * * * * * //
	// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

	{
		liner << local_prefix + ("Starting app, please wait...");

		if (conn.connect(url.c_str())) {

			Interface::Package get;

			while (conn.is_connected()) {
				liner << local_prefix + ("Connected. Verifying version...");
				conn.send_priority_package(compress(packages_id::VERSION_CHECK, communication_version));

				if (!conn.wait_for_package(std::chrono::milliseconds(10000))) {
					if (!conn.has_package()) continue;
				}

				get = conn.get_next();

				if (get.small_data.c_str()[0] != static_cast<char>(packages_id::VERSION_CHECK)) {
					liner << local_prefix + ("Failed to check version. Trying again...");
				}
				else break;
			}
						

			if (!conn.is_connected()) {
				liner << local_prefix + ("Lost connection. Please try again.");
				status.quitting = true;
				display->wait_for_n_draws(1);
			}

			// sure it is VERSION_CHECK
			if (get.small_data.c_str() + 1 != communication_version) {
				liner << local_prefix + ("Version mismatch. Host or client not up to date.");
				status.quitting = true;
				display->wait_for_n_draws(1);
			}
			else {
				liner << local_prefix + ("Connected to host successfully.");
				display->wait_for_n_draws(1);

				liner << local_prefix + ("Please type your name and press ENTER");

				{
					std::string ident;

					bool wait = true;
					userinput.ignore_strokes(false);
					userinput.on_enter([&, display](const std::string& str) {
						if (!str.empty()) {
							ident = str.substr(0, max_name_length);
							wait = false;
							userinput.ignore_strokes(true);
						}
					});

					while (wait) {
						std::this_thread::sleep_for(std::chrono::milliseconds(250));
						std::this_thread::yield();
					}

					conn.send_package(compress(packages_id::SET_NICK, ident));

					liner << local_prefix + ("You'll be known as '" + ident + "&f' in this session.");
					liner << local_prefix + ("Type '/help' if you don't know what to do.");

					current_user = ident;
					display->set_window_name(version_app + " | " + current_user + " @ " + current_channel);

					// enable user interaction

					display->refresh_all_forced();
					display->clear_all_to({ ' ' });
					//liner.force_update();
				}
			}
		}
		else {
			liner << local_prefix + ("Could not connect to " + url + ".");
			if (url == default_connection_url) liner << local_prefix + ("Please check for IPV6 connection.");
			liner << local_prefix + ("Please try again later or try calling the app with a custom IP (<this.exe> <ip>).");
			status.quitting = true;
			display->wait_for_n_draws(1);
		}
	}


	// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //
	// * * * * * * * * * * * * * * * * * * * / " " " " " " " " " " " " " " " " " " " " " " " " " " " " \ * * * * * * * * * * * * * * * * * * * //
	// * * * * * * * * * * * * * * * * * * * |                    USER INPUT HANDLE                    | * * * * * * * * * * * * * * * * * * * // 
	// * * * * * * * * * * * * * * * * * * * \ . . . . . . . . . . . . . . . . . . . . . . . . . . . . / * * * * * * * * * * * * * * * * * * * //
	// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //


	// Setup input handling	
	userinput.on_enter([&,display](const std::string& str) {

		std::unordered_map<size_t, std::string> processed;
		{
			std::string buf;
			size_t argp = 0;
			for (auto& i : str) {
				if (i != ' ') buf += i;
				else if (!buf.empty()) processed.emplace(argp++, std::move(buf));
			}
			if (!buf.empty()) processed.emplace(argp++, std::move(buf));
		}

		if (processed[0] == "/help") {
			liner << local_prefix + ("List of commands available in this version:");
			liner << local_prefix + ("&e/help                    &7| &fShows this help");
			liner << local_prefix + ("&e/quit                    &7| &fClose the app entirely");
			liner << local_prefix + ("&e/ping                    &7| &fShows current ping");
			liner << local_prefix + ("&e/nick <new name>         &7| &fChange your nickname");
			liner << local_prefix + ("&e/channel [channel_name]  &7| &fChange the channel you're in (no space). Empty for PUBLIC.");
			liner << local_prefix + ("&e/version                 &7| &fShows current version and other information");
			liner << local_prefix + ("&e/discord                 &7| &fShows from what Discord server this is from");
			liner << local_prefix + ("&e/message [message]       &7| &fGo to message mode and send [message] (optional)");
			liner << local_prefix + ("&e/list <page>             &7| &fAsks server who is waiting for /sendnow");
			liner << local_prefix + ("&e/send <short file path>  &7| &fTry to send file on channel (STRING PATH MAX SIZE: " + std::to_string(max_input_size) + ")");
			liner << local_prefix + ("&e/sendnow                 &7| &fIf someone accepted, start sending file now");
			liner << local_prefix + ("&e/close                   &7| &fTry to cancel and close any stuff going on + cleanup");
			liner << local_prefix + ("&e/receive                 &7| &fAccept someone sending file request on channel");
			liner << local_prefix + ("&e/debug                   &7| &fFull connection information");
			liner << local_prefix + ("&e<any text>               &7| &fGo to message mode and send a message");
		}
		else if (processed[0] == "/quit")
		{
			liner << local_prefix + ("Exiting the app...");
			status.quitting = true;
			userinput.ignore_strokes(true);
		}
		else if (processed[0] == "/ping")
		{
			liner << local_prefix + ("Your ping: " + std::to_string(conn.get_network_info().ping_now()) + " ms");
		}
		else if (processed[0] == "/debug")
		{
			const auto& net = conn.get_network_info();
			liner << local_prefix + "&2-&a ping_now:         &f" + std::to_string(net.ping_now()) + "&7 ms";
			liner << local_prefix + "&2-&a ping_peak:        &f" + std::to_string(net.ping_peak()) + "&7 ms";
			liner << local_prefix + "&2-&a ping_average:     &f" + std::to_string(net.ping_average_now()) + "&7 ms";
			liner << local_prefix + "&2-&a send_total:       &f" + Tools::byte_auto_string(net.send_get_total(), 3, "&7") + "&7B";
			liner << local_prefix + "&2-&a send_peak:        &f" + Tools::byte_auto_string(net.send_get_peak(), 3, "&7") + "&7B in one second";
			liner << local_prefix + "&2-&a send_bps_now:     &f" + Tools::byte_auto_string(net.send_get_current_bytes_per_second(), 3, "&7") + "&7B/s";
			liner << local_prefix + "&2-&a send_bps_alltime: &f" + Tools::byte_auto_string(net.send_get_average_total(), 3, "&7") + "&7B/s";
			liner << local_prefix + "&2-&a recv_total:       &f" + Tools::byte_auto_string(net.recv_get_total(), 3, "&7") + "&7B";
			liner << local_prefix + "&2-&a recv_peak:        &f" + Tools::byte_auto_string(net.recv_get_peak(), 3, "&7") + "&7B in one second";
			liner << local_prefix + "&2-&a recv_bps_now:     &f" + Tools::byte_auto_string(net.recv_get_current_bytes_per_second(), 3, "&7") + "&7B/s";
			liner << local_prefix + "&2-&a recv_bps_alltime: &f" + Tools::byte_auto_string(net.recv_get_average_total(), 3, "&7") + "&7B/s";
		}
		else if (processed[0] == "/nick")
		{
			
			if (processed[1].empty()) {
				liner << local_prefix + ("Failed to set new nickname.");
			}
			else {

				std::string combo;
				for (size_t p = 1; p < processed.size(); p++) combo += processed[p] + " ";
				if (combo.size()) combo.pop_back();

				if (!combo.empty()) {
					liner << local_prefix + ("Nickname update request to '" + combo + "&f' has been sent to the server.");

					conn.send_package(compress(packages_id::SET_NICK, combo));

					current_user = combo;
					display->set_window_name(version_app + " | " + current_user + " @ " + current_channel);
				}
				else {

					liner << local_prefix + ("Failed to set new nickname.");
				}
			}
		}
		else if (processed[0] == "/channel")
		{

			if (processed[1] == "") processed[1] = "PUBLIC"; // null is PUBLIC.

			processed[1] = processed[1].substr(0, max_name_length);

			liner.clear();
			display->refresh_all_forced();
			display->clear_all_to({ ' ' });
			
			conn.send_package(compress(packages_id::SET_CHANNEL, processed[1]));

			current_channel = processed[1];

			if (current_channel.empty()) {
				current_channel = "PUBLIC";
				liner << local_prefix + ("You're on PUBLIC channel (no ID channel)");
			}
			else {
				liner << local_prefix + ("You're on &f" + processed[1] + "&f channel now.");
			}
			display->set_window_name(version_app + " | " + current_user + " @ " + current_channel);
		}
		else if (processed[0] == "/version")
		{
			liner << local_prefix + ("App info: " + version_app);
			liner << local_prefix + ("Developed by: Lohkdesgds");
		}
		else if (processed[0] == "/discord")
		{
			liner << local_prefix + ("Discord link: " + discord_link);
		}
		else if (processed[0] == "/message")
		{
			if (!processed[1].empty()) {
				std::string combo;
				for (size_t p = 1; p < processed.size(); p++) combo += processed[p] + " ";
				if (combo.size()) combo.pop_back();

				if (!combo.empty()) {
					conn.send_priority_package(compress(packages_id::MESSAGE, combo.substr(0, max_input_size)));
				}
			}
		}
		else if (processed[0] == "/list")
		{
			conn.send_package(compress(packages_id::LIST_REQUEST, processed[1]));
		}
		else if (processed[0] == "/send")
		{
			/*
			Sending | Receiving

			FILE_ASK_SEND >>
			<< FILE_ACCEPT_REQUEST_SEND (receiver open file and wait)
			FILE_TRANSFER >> (sender open file and thread to send)
			FILE_TRANSFER_CLOSE >> (receiver close)
			*/
			if (status.on_transfer) {
				liner << local_prefix + ("It looks like you're on a transfer already. Cancel with /close?");
				return;
			}
			if (file_m.is_locked()) {
				liner << local_prefix + ("The file seems to be locked now. Try again later?");
				return;
			}

			if (file.is_open())
			{
				liner << local_prefix + ("You have a file open. Do you want to force /close it so you can open /send?");
				liner << local_prefix + ("/close will kill any task in progress.");
			}
			else if (!processed[1].empty()) {
				
				Tools::AutoLock l(file_m);

				std::string combo;
				for (size_t p = 1; p < processed.size(); p++) combo += processed[p] + " ";
				if (combo.size()) combo.pop_back();

				liner << local_prefix + ("Checking '" + combo + "'...");

				sprintf_s(last_request.filename, "%s", combo.c_str());

				if (!file.open(last_request.filename, Interface::smartfile::file_modes::READ)) {
					liner << local_prefix + ("Could not open this file. Please check incomplete or wrong path.");
					last_request = file_send_request{};
				}
				else {
					last_request.filesize = file.total_size();
					liner << local_prefix + ("File is now open and ready.");
					conn.send_package(compress(packages_id::FILE_ASK_SEND, Interface::transform_any_to_package(&last_request, sizeof(file_send_request))));
				}
			}
			else
			{
				liner << local_prefix + ("Empty path?! Do '/send <filename>'");
			}
		}
		else if (processed[0] == "/sendnow")
		{
			if (file_m.is_locked()) {
				liner << local_prefix + ("The file seems to be locked now. Try again later?");
				return;
			}

			{
				Tools::AutoLock l(file_m);

				if (status.on_transfer) {
					liner << local_prefix + ("You have a file transfer going on yet. Do you want to force /close it? (/close)");
					liner << local_prefix + ("/close will kill any task in progress.");
				}
				else if (strnlen_s(last_request.filename, max_file_name) <= 0) {
					liner << local_prefix + ("It seems that there's no file to send...?");
				}
				else if (!file.is_open()) {
					liner << local_prefix + ("It seems that there's no file opened yet... Try /send first?");
				}
			}

			file_send_thr.stop();
			file_send_thr.join();
			file_send_thr.set([&](Tools::boolThreadF b) {
				send_thread(b, conn, file, file_m, this_packages_total_count,/* signal_got,*/ file_transf_perc, last_request, status.on_transfer);
			});
			file_send_thr.start();

			liner << local_prefix + ("Transfering data!");
		}
		else if (processed[0] == "/close")
		{
			conn.send_package(compress(packages_id::FILE_TRANSFER_CLOSE, ""));
			liner << local_prefix + ("Removed self from server's list.");

			if (file_m.is_locked()) {
				liner << local_prefix + ("The file seems to be locked now. Try again later?");
				return;
			}
			Tools::AutoLock l(file_m);

			if (file.is_open()) {
				file.close();
				liner << local_prefix + ("Closed file successfully. If there was a transfer to you, file might be corrupted.");
			}
			liner << local_prefix + ("Cleanup done.");
			status.on_transfer = false;

			last_request = file_send_request{};
		}
		else if (processed[0] == "/receive")
		{
			if (file.is_open()) {
				liner << local_prefix + ("You have a file open already. Do you want to force /close it? (/close)");
				liner << local_prefix + ("/close will kill any task in progress.");
			}
			else if (strnlen_s(last_request.filename, max_file_name) <= 0) {
				liner << local_prefix + ("It seems that there's no file to receive...");
			}
			else if (file_m.is_locked()) {
				liner << local_prefix + ("The file seems to be locked now. Try again later?");
				return;
			}
			else {
				Tools::AutoLock l(file_m);

				for (auto& i : last_request.filename) {
					if (i == '/' || i == '\\') i = '%';
				}

				liner << local_prefix + (std::string("Opening result file '") + last_request.filename + "' to accept transfer...");

				if (file.open(last_request.filename, Interface::smartfile::file_modes::WRITE)) {

					liner << local_prefix + ("You are set to download the file now.");

					conn.send_package(compress(packages_id::FILE_ACCEPT_REQUEST_SEND, Interface::transform_any_to_package(&last_request, sizeof(file_send_request)))); // CHECK
				}

			}
		}
		else if (str.find("/") == 0) {

			liner << local_prefix + "This command doesn't exist. Sorry.";
		}
		else if (!str.empty())
		{
			//liner.force_update();
			conn.send_priority_package(compress(packages_id::MESSAGE, str.substr(0, max_input_size)));
		}

	});


	// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //
	// * * * * * * * * * * * * * * * * * * * / " " " " " " " " " " " " " " " " " " " " " " " " " " " " \ * * * * * * * * * * * * * * * * * * * //
	// * * * * * * * * * * * * * * * * * * * |               RECEIVE AUTOMATIC HANDLING                | * * * * * * * * * * * * * * * * * * * // 
	// * * * * * * * * * * * * * * * * * * * \ . . . . . . . . . . . . . . . . . . . . . . . . . . . . / * * * * * * * * * * * * * * * * * * * //
	// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

	Tools::SuperThread<> request_update{Tools::superthread::performance_mode::EXTREMELY_LOW_POWER};
	request_update.set([&](Tools::boolThreadF run) {
		while (run()) {
			conn.send_package(compress(packages_id::REQUEST_TYPING, ""));
			std::this_thread::sleep_for(Custom::time_typing_limit);
		}
	});
	request_update.start();


	// handle packages self
	conn.overwrite_reads_to([&](const Interface::Connection& me, Interface::Package& combining) {

		try{
			uintptr_t thus = (uintptr_t)&me;

			std::string& data = combining.small_data;

			if (data.size() < 1) return;
			if (!thus) return;

			const char* data_ahead = data.data() + 1;

			/*if (high_performance_needed) {
				me.set_mode(Tools::superthread::performance_mode::HIGH_PERFORMANCE);
				high_performance_needed--;
			}
			else me.set_mode(Tools::superthread::performance_mode::BALANCED);*/

			switch (static_cast<packages_id>(data[0])) {
			case packages_id::MESSAGE_RECEIVED:
			{
				liner << data_ahead;
			}
				break;
			case packages_id::TYPING:
			{
				last_typing_amount = atoi(data_ahead);
			}
				break;
			/*case packages_id::SIGNAL:
			{
				signal_got = true;
			}
				break;*/
			case packages_id::SERVER_MESSAGE:
			{
				liner << server_prefix + data_ahead;
			}
				break;
			case packages_id::FILE_ASK_SEND:
			{
				/*
				Sending | Receiving

				FILE_ASK_SEND >>
				<< FILE_ACCEPT_REQUEST_SEND (receiver open file and wait)
				FILE_TRANSFER >> (sender open file and thread to send)
				FILE_TRANSFER_CLOSE >> (receiver close)
				*/

				//liner.force_update();

				if (status.on_transfer) {
					liner << local_prefix + "Someone wants to send a file, but you're on a transfer already, so you can't accept it.";
				}
				else if (data.size() == sizeof(file_send_request) + 1) {

					last_request = file_send_request{};
					Interface::transform_any_package_back(&last_request, sizeof(file_send_request), data.substr(1));

					if (last_request.filesize && strnlen_s(last_request.filename, max_file_name) != 0 && last_request._origin_transfer != 0) {
						liner << local_prefix + last_request.from + " &fwants to send a file named '" + last_request.filename + "' (" + Tools::byte_auto_string(last_request.filesize, 2) + "B)";
						liner << local_prefix + "Type &a/receive&f to say you want it.";
					}
					else {
						liner << local_prefix + "&cA malformed file transfer request was received!";
						last_request = file_send_request{};
					}
				}
				else {
					liner << local_prefix + "&cA malformed file transfer request was received!";
					last_request = file_send_request{};
				}
			}
				break;
			case packages_id::FILE_TRANSFER: // me receive file
			{
				if (!status.on_transfer) {
					liner << local_prefix + "&aTransfering has initialized.";
				}
				status.on_transfer = true;
				//high_performance_needed = high_performance_cooldown;

				//Tools::AutoLock l(file_m);

				if (!file.is_open()) {
					liner << local_prefix + "&4[EXCEPTION]&c Someone tried to send you file data. Requesting server to stop.";

					//liner.force_update();

					conn.send_package(compress(packages_id::FILE_TRANSFER_CLOSE, ""));
					break;
				}
				else if (auto currdata = data.substr(1); currdata.size()) {
					this_packages_total_count+= currdata.size();
					file.write(currdata);

					file_transf_perc = file.total_size() * 1.0 / last_request.filesize;
				}
			}
				break;
			case packages_id::FILE_TRANSFER_CANCEL_NO_USER: // if me sending, cancel send
			{
				status.on_transfer = false;

				file_send_thr.stop();
			}
				break;
			case packages_id::FILE_TRANSFER_CLOSE: // me close file
			{
				if (status.on_transfer) {
					liner << local_prefix + "&aTransfering is ending.";
				}
				status.on_transfer = false;

				Tools::AutoLock l(file_m);

				//liner.force_update();

				if (!file.is_open()) {
					liner << local_prefix + "&4[EXCEPTION] &cServer asked to close file, but no file is open.";
				}
				else {
					file.close();
					liner << local_prefix + "&aTransfer complete.";
				}

				last_request = file_send_request{};
			}
				break;
			}
		}
		catch (const std::exception& e) {
			liner << local_prefix + "&4[EXCEPTION] &cFatal exception (may break file transfer): " + e.what();
		}
		catch (...) {
			liner << local_prefix + "&4[EXCEPTION] &cUNKNOWN FATAL EXCEPTION (may break file transfer): ";
		}
	});


	// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //
	// * * * * * * * * * * * * * * * * * * * / " " " " " " " " " " " " " " " " " " " " " " " " " " " " \ * * * * * * * * * * * * * * * * * * * //
	// * * * * * * * * * * * * * * * * * * * |                    END OF MAIN CODE                     | * * * * * * * * * * * * * * * * * * * // 
	// * * * * * * * * * * * * * * * * * * * \ . . . . . . . . . . . . . . . . . . . . . . . . . . . . / * * * * * * * * * * * * * * * * * * * //
	// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

	// enable user interaction
	userinput.ignore_strokes(false);
	display->refresh_all_forced();

	bool once_joined = false;

	// wait for the end
	while (!status.quitting && conn.is_connected()) {
		once_joined = true;
		std::this_thread::sleep_for(std::chrono::milliseconds(250));
		std::this_thread::yield();
	}

	// end threads lmao
	request_update.stop();
	file_send_thr.stop();


	if (!conn.is_connected() && once_joined) {
		display->clear_all_to({ ' ' });
		liner << local_prefix + "&cConnection LOST! You or the server got offline. Exiting the app...";
	}

	display->wait_for_n_draws(1);
	display.reset();

	std::this_thread::sleep_for(std::chrono::seconds(5));

	return 0;
}

std::string compress(const packages_id id, const std::string& str)
{
	return std::string(static_cast<char>(id) + str);
}


void send_thread(Tools::boolThreadF run, Interface::Connection& conn, Interface::SmartFile& file, Tools::SuperMutex& file_m, size_t& packages_total_count, /*bool& signal,*/ double& perc, file_send_request file_info, bool& on_transf)
{
	if (!conn.is_connected()) return;
	if (!file.is_open()) return;
	if (file_m.is_locked()) return;

	try {
		Tools::AutoLock l(file_m);

		packages_total_count = 0;

		on_transf = true;

		//conn.send_package(std::move(file));

		while (run()) {
			std::string buf;
			if (file.read(buf, Interface::connection::package_size) <= 0) {
				conn.send_package(compress(packages_id::FILE_TRANSFER_CLOSE, ""));
				file.close();
				on_transf = false;
				return;
			}

			packages_total_count += buf.size();
			conn.send_package(compress(packages_id::FILE_TRANSFER, buf));

			perc = (packages_total_count * 1.0) / file_info.filesize;

//			packages_total_count+= buf.size();
		}
		on_transf = false;

		perc = 1.0;

		// on stop, abort and close.
		file.close();
		conn.send_package(compress(packages_id::FILE_TRANSFER_CLOSE, ""));
	}
	catch (...) {
		on_transf = false;
		file.close();
		conn.send_package(compress(packages_id::FILE_TRANSFER_CLOSE, ""));
	}


	/*if (!conn.is_connected()) return;
	if (!file.is_open()) return;
	if (file_m.is_locked()) return;


	try {
		Tools::AutoLock l(file_m);

		Interface::MemoryFile memfile;
		if (!memfile.clone(file)) {
			debug("send_thread FAILED to clone from SMARTFILE!");
			perc = -1.0;
			return;
		}

		conn.send_package(std::move(memfile));

		perc = 1.0;

		// on stop, abort and close.
		file.close();
		conn.send_package(compress(packages_id::FILE_TRANSFER_CLOSE, ""));
	}
	catch (...) {
		file.close();
		conn.send_package(compress(packages_id::FILE_TRANSFER_CLOSE, ""));
	}*/
}



// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //
// * * * * * * * * * * * * * * * * * * * / " " " " " " " " " " " " " " " " " " " " " " " " " " " " \ * * * * * * * * * * * * * * * * * * * //
// * * * * * * * * * * * * * * * * * * * |                     HOSTING SERVER                      | * * * * * * * * * * * * * * * * * * * // 
// * * * * * * * * * * * * * * * * * * * \ . . . . . . . . . . . . . . . . . . . . . . . . . . . . / * * * * * * * * * * * * * * * * * * * //
// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //



int as_host()
{
	std::shared_ptr<Custom::CmdDisplay<WIDTH, HEIGHT>> display = std::make_shared<Custom::CmdDisplay<WIDTH, HEIGHT>>();
	Custom::MultiLiner liner;

	display->draw_at(0, 0, "Please wait...");

	display->add_drawing_func([](int x, int y, Tools::char_c& ch)->bool {
		if (x == -1) return true;
		if ((y == 1)) ch = (x % 2 == 0) ? Tools::char_c{ '*', Tools::cstring::C::DARK_GRAY } : Tools::char_c{ ' ' };
		return false;
	});

	liner.set_func_draw([display](int py, const Tools::Cstring& str) {
		display->draw_at(py + 2, 0, empty_line);
		display->draw_at(py + 2, 0, str);
	});
	liner.set_line_amount(HEIGHT - 3);
	liner.set_line_width_max(WIDTH - 2);

	liner << "Starting host...";

	Interface::Hosting host { true };
	Custom::Channels channels;
	Custom::TransferHelp transfers_ongoing;
	double _a_tb_s = 0, _a_tb_r = 0;


	host.set_connections_limit(20); // 0 = unlimited


	display->add_tick_func([&](Custom::CmdDisplay<WIDTH, HEIGHT>& thus) {
		
		double _tb_s = _a_tb_s, _tb_r = _a_tb_r;

		for (size_t p = 0; p < host.size(); p++) {
			auto u = host.get_connection(p);
			_tb_r += static_cast<double>(u->get_network_info().recv_get_total());
			_tb_s += static_cast<double>(u->get_network_info().send_get_total());
		}

		thus.draw_at(0, 0, 
			std::string("&8[ &2CONNECTED &8][ &7TOTAL BANDWITH UPLOAD: ") + Tools::byte_auto_string(_tb_s, 2) + "B &8|&7 DOWNLOAD: " + Tools::byte_auto_string(_tb_r, 2) + "B &8]" + empty_line
			,false);
		display->set_window_name(default_naming_top + "Hosting! " + std::to_string(host.size()) + " connected");
	});

	if (!host.is_connected()) {
		display->set_window_name(default_naming_top + "Failed to open host!");
		liner << "&cCould not open host on default port &4" + std::to_string(Interface::connection::default_port) + "&c.";
		display->wait_for_n_draws(1);
		display.reset();
		std::this_thread::sleep_for(std::chrono::seconds(3));
		return 0;
	}

	liner << "Started host on port &e" + std::to_string(Interface::connection::default_port) + "&f successfully! Waiting new connections...";

	display->refresh_all_forced();


	// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //
	// * * * * * * * * * * * * * * * * * * * / " " " " " " " " " " " " " " " " " " " " " " " " " " " " \ * * * * * * * * * * * * * * * * * * * //
	// * * * * * * * * * * * * * * * * * * * |                AUTOMATIC PASSIVE CONTROL                | * * * * * * * * * * * * * * * * * * * // 
	// * * * * * * * * * * * * * * * * * * * \ . . . . . . . . . . . . . . . . . . . . . . . . . . . . / * * * * * * * * * * * * * * * * * * * //
	// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //


	//size_t high_performance_needed = high_performance_cooldown;

	auto function_command = [&](Interface::Connection& me, Interface::Package& datada) {
		//liner << "&5[DEBUG] " + data;
		uintptr_t thus = (uintptr_t)&me;

		{
			bool found = false;
			for (size_t p = 0; p < host.size(); p++)
			{
				auto i = host.get_connection(p);
				if ((uintptr_t)i.get() == thus) {
					found = true;
					break;
				}
			}
			if (!found) return;
		}

		std::string& data = datada.small_data;

		const char* data_ahead = data.data() + 1;


		/*if (high_performance_needed) {
			me.set_mode(Tools::superthread::performance_mode::HIGH_PERFORMANCE);
			high_performance_needed--;
		}
		else me.set_mode(Tools::superthread::performance_mode::BALANCED);*/


		switch (static_cast<packages_id>(data[0])) {
		case packages_id::VERSION_CHECK:
		{
			me.send_package(compress(packages_id::VERSION_CHECK, communication_version));

			if (communication_version != data_ahead) {
				me.close();

				liner << Tools::sprintf_a("&5[@%s&5]&8{%08X}&f handshake version &cFAILED&f.", channels.channel_of(thus).c_str(), thus);
				//liner << Tools::sprintf_a("{%08X}", thus) + " handshake version FAILED.";
				return;
			}
			else {

				liner << Tools::sprintf_a("&5[@%s&5]&8{%08X}&f handshake version &aOK&f.", channels.channel_of(thus).c_str(), thus);
				//liner << Tools::sprintf_a("{%08X}", thus) + " handshake version OK.";
			}
		}
			break;/*
		case packages_id::SIGNAL:
		{
			me.send_package(compress(packages_id::SIGNAL, "")); // no overload way
		}
			break;*/
		case packages_id::TYPING:
		{
			channels.set_typing(thus);
		}
			break;
		case packages_id::REQUEST_TYPING:
		{
			auto count = channels.amount_of_users_typing(channels.channel_of(thus));
			me.send_package(compress(packages_id::TYPING, std::to_string(count)));
		}
			break;
		case packages_id::SET_NICK:
		{
			if (strnlen_s(data_ahead, data.length() - 1) == 0) break;

			auto old_nick = channels.nick_of(thus);

			if (old_nick == data_ahead) break;

			channels.set_nick(thus, data_ahead);
			auto mee_ch = channels.channel_of(thus);

			liner << Tools::sprintf_a("&5[@%s&5]&8{%08X}&f %s&f changed name to %s&f.", channels.channel_of(thus).c_str(), thus, old_nick.c_str(), channels.nick_of(thus).c_str());
			//liner << Tools::sprintf_a("{%08X}", thus) + " changed nick: " + old_nick + " -> " + data_ahead;

			auto l = channels.list(mee_ch);

			if (!old_nick.empty()) {
				for (auto& i : l) {
					if (!i.id_ptr.expired()) {
						auto thm = i.id_ptr.lock();
						thm->send_package(compress(packages_id::SERVER_MESSAGE, Tools::sprintf_a("&f%s&f changed nickname to &f%s&f", old_nick.c_str(), data_ahead)));
					}
				}
			}
			else {
				for (auto& i : l) {
					if (!i.id_ptr.expired()) {
						auto thm = i.id_ptr.lock();
						thm->send_package(compress(packages_id::SERVER_MESSAGE, Tools::sprintf_a("&fSomeone just appeared as &f%s&f!", data_ahead)));
					}
				}
			}
		}
			break;
		case packages_id::MESSAGE:
		{
			if (strnlen_s(data_ahead, data.length() - 1) == 0) break;

			auto mee_ch = channels.channel_of(thus);
			auto l = channels.list(mee_ch);

			//liner << Tools::sprintf_a("[@%s]{%08X} %s: %s", channels.channel_of(thus).c_str(), thus, channels.nick_of(thus).c_str(), data_ahead);
			//liner << "[@" + mee_ch + "]" + Tools::sprintf_a("{%08X}", thus) + " (" + channels.nick_of(thus) + ") sent: " + data_ahead;

			for (auto& i : l) {
				if (!i.id_ptr.expired()) {
					auto thm = i.id_ptr.lock();
					thm->send_package(compress(packages_id::MESSAGE_RECEIVED, Tools::sprintf_a("&f%s&8: &7%s", channels.nick_of(thus).c_str(), data_ahead)));
				}
			}
		}
			break;
		case packages_id::SET_CHANNEL:
		{
			auto old_ch = channels.channel_of(thus);
			std::string curr_channel = (data_ahead ? data_ahead : "PUBLIC");
			curr_channel = curr_channel.substr(0, max_name_length);
			channels.set(thus, curr_channel);

			liner << Tools::sprintf_a("&5[@%s&5]&8{%08X}&f %s&f is now on channel %s&f.", channels.channel_of(thus).c_str(), thus, channels.nick_of(thus).c_str(), curr_channel.c_str());
			//liner << Tools::sprintf_a("{%08X}", thus) + " changed to channel '" + curr_channel + "'";

			{
				auto l = channels.list(old_ch);
				for (auto& i : l) {
					if (!i.id_ptr.expired()) {
						auto thm = i.id_ptr.lock();
						thm->send_package(compress(packages_id::SERVER_MESSAGE, Tools::sprintf_a("&f%s &eleft the channel.", channels.nick_of(thus).c_str())));
					}
				}
			}

			{
				auto l = channels.list(curr_channel);
				for (auto& i : l) {
					if (!i.id_ptr.expired()) {
						auto thm = i.id_ptr.lock();
						thm->send_package(compress(packages_id::SERVER_MESSAGE, Tools::sprintf_a("&f%s &ejoined the channel.", channels.nick_of(thus).c_str())));
					}
				}
			}
		}
			break;
		case packages_id::LIST_REQUEST:
		{
			auto getting = transfers_ongoing.list_transf(thus);

			auto offset = atoll(data_ahead);
			size_t real_offset = items_per_list * offset;
			
			if (real_offset >= getting.size()) {
				me.send_package(compress(packages_id::SERVER_MESSAGE, "There's no one waiting for your file."));
			}
			else {
				me.send_package(compress(packages_id::SERVER_MESSAGE, Tools::sprintf_a("List of users in send queue [&e%zu&f-&e%zu&f]:", real_offset, (real_offset + items_per_list > getting.size()) ? getting.size() : (real_offset + items_per_list))));
			}

			for (size_t p = real_offset; p < getting.size() && (p - real_offset) < items_per_list; p++) {
				me.send_package(compress(packages_id::SERVER_MESSAGE, Tools::sprintf_a("[&6%zu&f] &f%s", p, channels.nick_of(getting[p]).c_str())));
			}
		}
			break;
		case packages_id::FILE_ASK_SEND:
		{
			/*
			Sending | Receiving

			FILE_ASK_SEND >>
			<< FILE_ACCEPT_REQUEST_SEND (receiver open file and wait)
			FILE_TRANSFER >> (sender open file and thread to send)
			FILE_TRANSFER_CLOSE >> (receiver close)
			*/

			if (data.size() < sizeof(file_send_request) + 1) {
				
				liner << Tools::sprintf_a("&5[@%s&5]&8{%08X}&f %s bad request &cFILE_ASK_SEND&f.", channels.channel_of(thus).c_str(), thus, channels.nick_of(thus).c_str());
				//liner << Tools::sprintf_a("{%08X}", thus) + " " + channels.nick_of(thus) + " bad request FILE_ASK_SEND.";

				me.send_package(compress(packages_id::SERVER_MESSAGE, "&4[ERROR] Bad request. Communication data was lost or corrupted. Try again."));
			}

			auto mee_ch = channels.channel_of(thus);
			auto l = channels.list(mee_ch);

			file_send_request protocol;
			Interface::transform_any_package_back(&protocol, sizeof(file_send_request), data.substr(1));

			liner << Tools::sprintf_a("&5[@%s&5]&8{%08X}&f %s&f dropped file in chat &8{%s;%sB}&f.", channels.channel_of(thus).c_str(), thus, channels.nick_of(thus).c_str(), protocol.filename, Tools::byte_auto_string(protocol.filesize, 2).c_str());
			//liner << "[@" + mee_ch + "]" + Tools::sprintf_a("{%08X}", thus) + " " + channels.nick_of(thus) + " is about to send '" + protocol.filename + "' - " + Tools::byte_auto_string(protocol.filesize, 2) + "B";

			protocol._origin_transfer = thus;
			sprintf_s(protocol.from, "%s", channels.nick_of(thus).c_str());

			for (auto& i : l) {
				if (!i.id_ptr.expired() && i.id_i != thus) {
					auto thm = i.id_ptr.lock();
					thm->send_package(compress(packages_id::FILE_ASK_SEND, Interface::transform_any_to_package(&protocol, sizeof(file_send_request))));
				}
			}

			me.send_package(compress(packages_id::SERVER_MESSAGE, "&6Users now know about your file."));
		}
			break;
		case packages_id::FILE_ACCEPT_REQUEST_SEND:
		{
			if (data.size() < sizeof(file_send_request) + 1) {
				liner << Tools::sprintf_a("&5[@%s&5]&8{%08X}&f %s&f bad request &cFILE_ACCEPT_REQUEST_SEND&f.", channels.channel_of(thus).c_str(), thus, channels.nick_of(thus).c_str());
				//liner << Tools::sprintf_a("{%08X}", thus) + " " + channels.nick_of(thus) + " bad request FILE_ACCEPT_REQUEST_SEND.";

				me.send_package(compress(packages_id::SERVER_MESSAGE, "[ERROR] Bad request. Communication data was lost or corrupted. Try again."));
			}

			file_send_request protocol;
			Interface::transform_any_package_back(&protocol, sizeof(file_send_request), data.substr(1));

			if (auto the_transferer = channels.get_user(protocol._origin_transfer); the_transferer) {
				
				if (transfers_ongoing.add_me_on_sender(thus, protocol._origin_transfer)) {
					auto mee_ch = channels.channel_of(thus);
					auto l = channels.list(mee_ch);

					liner << Tools::sprintf_a("&5[@%s&5]&8{%08X}&f %s&f wants stream from &8{%08X}", channels.channel_of(thus).c_str(), thus, channels.nick_of(thus).c_str(), protocol._origin_transfer);
					//liner << "[@" + mee_ch + "]" + Tools::sprintf_a("{%08X}", thus) + " (" + channels.nick_of(thus) + ") added self to file transfer from " + Tools::sprintf_a("{%08X}", protocol._origin_transfer);

					for (auto& i : l) {
						if (!i.id_ptr.expired()) {
							auto thm = i.id_ptr.lock();
							thm->send_package(compress(packages_id::SERVER_MESSAGE, Tools::sprintf_a("&b%s&f has added self for &b%s&f's file transfer.", channels.nick_of(thus).c_str(), channels.nick_of(protocol._origin_transfer).c_str())));
						}
					}
				}
				else {
					me.send_package(compress(packages_id::SERVER_MESSAGE, "&4[ERROR] File transfer already started, finished or unavailable. Sorry."));
				}
			}
			else {
				me.send_package(compress(packages_id::SERVER_MESSAGE, "&4[ERROR] Could not find a file transfer ongoing."));
			}
		}
			break;
		case packages_id::FILE_TRANSFER:
		{
			transfers_ongoing.set_transfering(thus);

			auto getting = transfers_ongoing.list_transf(thus);

			//me.set_mode(Tools::superthread::performance_mode::HIGH_PERFORMANCE);

			if (getting.empty()) {
				me.send_package(compress(packages_id::FILE_TRANSFER_CANCEL_NO_USER, ""));
				me.send_package(compress(packages_id::SERVER_MESSAGE, "&4[ERROR] No user to transfer file found. No one did /receive or they got offline."));

				transfers_ongoing.cleanup_user(thus);
			}
			else {
				for (auto& i : getting) {
					auto user = channels.get_user(i);
					if (user) {
						//user->set_mode(Tools::superthread::performance_mode::HIGH_PERFORMANCE);
						user->send_package(data); // direct transfer, no copy or bs.
					}
				}
			}
		}
			break;
		case packages_id::FILE_TRANSFER_CLOSE:
		{
			//me.set_mode(Tools::superthread::performance_mode::BALANCED);

			liner << Tools::sprintf_a("&5[@%s&5]&8{%08X}&f %s&f closed a file stream.", channels.channel_of(thus).c_str(), thus, channels.nick_of(thus).c_str());
			// list is generated only if user did /send
			auto getting = transfers_ongoing.list_transf(thus);

			me.send_package(compress(packages_id::SERVER_MESSAGE, "&aYour file transfer ended!"));

			//me.set_mode(Tools::superthread::performance_mode::BALANCED);

			if (!getting.empty()) {
				for (auto& i : getting) {
					auto user = channels.get_user(i);
					if (user) {
						//user->set_mode(Tools::superthread::performance_mode::BALANCED);
						user->send_package(compress(packages_id::FILE_TRANSFER_CLOSE, "")); // close.
						user->send_package(compress(packages_id::SERVER_MESSAGE, Tools::sprintf_a("&b%s &aended file transfer!", channels.nick_of(thus).c_str())));
					}
				}
			}

			transfers_ongoing.cleanup_user(thus);
		}
			break;
		}

	};


	// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //
	// * * * * * * * * * * * * * * * * * * * / " " " " " " " " " " " " " " " " " " " " " " " " " " " " \ * * * * * * * * * * * * * * * * * * * //
	// * * * * * * * * * * * * * * * * * * * |                    SIMPLE PEER START                    | * * * * * * * * * * * * * * * * * * * // 
	// * * * * * * * * * * * * * * * * * * * \ . . . . . . . . . . . . . . . . . . . . . . . . . . . . / * * * * * * * * * * * * * * * * * * * //
	// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //


	host.on_new_connection([&](std::shared_ptr<Interface::Connection> nc) {
		//nc->set_max_buffering(max_buffer);


		nc->debug_error_function([&](const std::string& err) {liner << local_prefix + "&cSomething bad happened: " + err; });

		liner << Tools::sprintf_a("&5[@PUBLIC&5]&8{%08X}&f is joining the server...", (uintptr_t)nc.get());
		channels.add(nc);

		nc->set_mode(Tools::superthread::performance_mode::BALANCED);

		auto mee_ch = channels.channel_of((uintptr_t)nc.get());
		auto l = channels.list(mee_ch);
		for (auto& i : l) {
			if (!i.id_ptr.expired() && i.id_i != (uintptr_t)nc.get()) {
				auto thm = i.id_ptr.lock();
				thm->send_package(compress(packages_id::SERVER_MESSAGE, Tools::sprintf_a("&8Someone is connecting...", (uintptr_t)nc.get())));
			}
		}

		nc->overwrite_reads_to(function_command);
	});
	host.on_connection_close([&](uintptr_t id) {
		liner << Tools::sprintf_a("&5[@%s&5]&8{%08X}&f lost connection", channels.channel_of(id).c_str(), id);

		Interface::Connection* quick = (Interface::Connection*)id;
		_a_tb_r += static_cast<double>(quick->get_network_info().recv_get_total());
		_a_tb_s += static_cast<double>(quick->get_network_info().send_get_total());

		auto curr_channel = channels.channel_of(id);
		auto l = channels.list(curr_channel);
		for (auto& i : l) {
			if (!i.id_ptr.expired()) {
				auto thm = i.id_ptr.lock();
				thm->send_package(compress(packages_id::SERVER_MESSAGE, Tools::sprintf_a("&8%s disconnected.", (channels.nick_of(id).empty() ? "Someone" : channels.nick_of(id)).c_str())));
			}
		}

		channels.remove(id);
		transfers_ongoing.cleanup_user(id);
		//memorymng.remove(id);
	});

	// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //
	// * * * * * * * * * * * * * * * * * * * / " " " " " " " " " " " " " " " " " " " " " " " " " " " " \ * * * * * * * * * * * * * * * * * * * //
	// * * * * * * * * * * * * * * * * * * * |                       END OF HOST                       | * * * * * * * * * * * * * * * * * * * // 
	// * * * * * * * * * * * * * * * * * * * \ . . . . . . . . . . . . . . . . . . . . . . . . . . . . / * * * * * * * * * * * * * * * * * * * //
	// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

	while (host.is_connected()) {
		std::this_thread::sleep_for(std::chrono::milliseconds(250));
		std::this_thread::yield();
	}

	return 0;
}