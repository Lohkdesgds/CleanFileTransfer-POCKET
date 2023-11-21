#include <Lunaris/socket/socket.h>
#include <Lunaris/console/console.h>
#include <Lunaris/encryption/encryption.h>

#include <string.h>
#include <fstream>

using namespace Lunaris;

const u_short app_port = 55223;
const char app_version[] = "V0.0.1 CFT w/ Encryption CMD direct version";

char* get_arg(int, int, char**);
int as_host(const char*, const char*);
int as_client(const char*, const char*);
bool send_file_thru(TCP_client&, const char*);
bool recv_file_thru(TCP_client&);

struct block_data {
	uint64_t len;
};

/*
* ARGS (client):
* 1: "client"
* 2: ip
* 3: file_to_send (optional, receives if empty)
* ARGS (host):
* 1: "host"
* 2: ip (expected to connect or "NULL")
* 3: file_to_send (optional) 
*/
int main(int argc, char* argv[])
{
	cout << console::color::GRAY << app_version;

	const char* a_type    = get_arg(1, argc, argv);
	const char* a_ip      = get_arg(2, argc, argv);
	const char* a_fp_send = get_arg(3, argc, argv);

	if (strcmp(a_type, "client") == 0) {
		return as_client(a_ip, a_fp_send);
	}
	else if (strcmp(a_type, "host") == 0) {
		return as_host(a_ip, a_fp_send);
	}
	else {
		cout << console::color::YELLOW << "You should call the app with args: [client | host] [<ip> | NULL] <file>";
	}
	return 0;
}

int as_host(const char* ip, const char* file)
{
	Lunaris::TCP_host host;
	if (!host.setup(socket_config().set_port(app_port))) {
		cout << console::color::RED << "Failed to open host at port " << app_port;
		return -1;
	}

	cout << console::color::GREEN << "Opened host at port " << app_port << ". Waiting...";

	Lunaris::TCP_client cli;
	while (1) {
		cli = host.listen();
		if (strcmp(ip, "NULL") != 0 && cli.info().ip_address != ip) {
			cout << console::color::YELLOW << "Someone tried to connect from " << cli.info().ip_address << ", but expected from " << ip << ". Disconnected, waiting again.";
			cli.close_socket();
			continue;
		}
		break;
	}
	host.close_socket();

	cout << console::color::GREEN << "Someone connected from " << cli.info().ip_address << ". Working on it...";

	std::thread thr_recv([&] {
		recv_file_thru(cli);
	});

	send_file_thru(cli, file);

	thr_recv.join();

	return 0;
}
int as_client(const char* ip, const char* file)
{
	Lunaris::TCP_client cli;
	if (!cli.setup(socket_config().set_port(app_port).set_ip_address(ip))) {
		cout << console::color::RED << "Failed to connect to host " << ip;
		return -1;
	}

	cout << console::color::GREEN << "Connected to " << ip << ". Working on it...";
	
	std::thread thr_recv([&] {
		recv_file_thru(cli);
	});

	send_file_thru(cli, file);

	thr_recv.join();

	return 0;
}

bool send_block_info(TCP_client& c, block_data bd) {
	return c.send((char*) &bd, sizeof(bd));
}
bool send_file_thru(TCP_client& c, const char* f)
{
	if (!f) {
		c.send("\0", 1);
		return false;
	}

	std::fstream fp(f, std::ios::in | std::ios::binary);
	if (!fp.is_open() || !fp.good() || !fp) {
		cout << console::color::LIGHT_PURPLE << "Nothing to send.";
		return false;
	}

	auto enc = Lunaris::make_encrypt_auto();
	const auto key_foreign = enc.get_combo();

	cout << console::color::LIGHT_PURPLE << "Sending encryption keys...";

	send_block_info(c, { sizeof(key_foreign)});
	c.send((char*)&key_foreign, sizeof(key_foreign));

	cout << console::color::LIGHT_PURPLE << "Sending info about " << f << "...";

	send_block_info(c, { strlen(f) + 1 });
	c.send(f, strlen(f) + 1);

	cout << console::color::LIGHT_PURPLE << "Sending " << f << "...";

	std::vector<char> vec;
	vec.resize((size_t)1 << 17);
	std::vector<uint8_t> ee;

	std::atomic<size_t> counter = 0, counter_enc = 0;
	std::atomic<bool> ended = false;
	
	std::thread report([&counter, &counter_enc, &ended] {
		while (!ended) {
			std::this_thread::sleep_for(std::chrono::seconds(1));
			cout << console::color::LIGHT_PURPLE << "SEND: " << (counter.exchange(0) * 0.000001) << " MB/s. (real usage: " << (counter_enc.exchange(0) * 0.000001) << " MB/s)";
		}
	});

	while (!fp.eof()) {
		fp.read(vec.data(), vec.size());
		const size_t gg = fp.gcount();
		if (gg == 0) break;

		enc.transform((uint8_t*)vec.data(), gg, ee);

		send_block_info(c, { ee.size() });
		c.send((char*)ee.data(), ee.size());

		counter += gg;
		counter_enc += ee.size();
	}
	send_block_info(c, { 0 });

	ended = true;
	report.join();

	cout << console::color::LIGHT_PURPLE << "Ended!";

	return true;
}
bool recv_block(TCP_client& c, std::vector<char>& targ)
{
	const auto i = c.recv(sizeof(block_data));
	if (i.size() != sizeof(block_data)) return false;

	const block_data& bd = *((block_data*)i.data());
	if (bd.len == 0) return false;

	targ = c.recv(bd.len);
	return true;
}
bool recv_file_thru(TCP_client& c) {
	cout << console::color::BLUE << "Checking if there's something to receive...";

	std::vector<char> buf;

	cout << console::color::BLUE << "Getting encryption keys...";

	if (!recv_block(c, buf)) {
		cout << console::color::BLUE << "Nothing to download or failed.";
		return false;
	}

	const auto dec = make_decrypt_auto(*((RSA_keys<uint64_t>*)buf.data()));

	cout << console::color::BLUE << "Getting file name...";


	if (!recv_block(c, buf)) {
		cout << console::color::BLUE << "Nothing to download or failed.";
		return false;
	}

	const std::string fpstr = buf.data();

	cout << console::color::BLUE << "Preparing to download " << fpstr << "...";

	std::fstream fp(fpstr, std::ios::out | std::ios::binary);
	if (!fp.is_open() || !fp.good() || !fp) {
		cout << console::color::BLUE << "Failed to open file to write.";
		return false;
	}

	cout << console::color::BLUE << "Writing on disk...";

	std::vector<uint8_t> dd;
	std::atomic<size_t> counter = 0, counter_enc = 0;
	std::atomic<bool> ended = false;

	std::thread report([&counter, &counter_enc, &ended] {
		while (!ended) {
			std::this_thread::sleep_for(std::chrono::seconds(1));
			cout << console::color::BLUE << "RECV: " << (counter.exchange(0) * 0.000001) << " MB/s. (real usage: " << (counter_enc.exchange(0) * 0.000001) << " MB/s)";
		}
	});

	while (recv_block(c, buf)) {
		dec.transform((uint8_t*)buf.data(), buf.size(), dd);
		counter += dd.size();
		counter_enc += buf.size();
		fp.write((char*)dd.data(), dd.size());
	}

	ended = true;
	report.join();

	cout << console::color::BLUE << "Ended download!";
	fp.close();
}

char* get_arg(int idx, int len, char** src) {
	static char empty_str[] = "";
	return idx >= len ? (char*)empty_str : src[idx];
}