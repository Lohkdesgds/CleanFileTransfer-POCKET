#include "synced.h"

void SyncedControl::async_send()
{
	cout_ctl(message_type::SYSTEM, "Sending stuff enabled.");

	const auto f_send = [this](const package_commu::type t = package_commu::type::SYNC) {
		package_commu pkg;
		pkg.typ = t;
		client.send((char*)&pkg, sizeof(pkg));
	};
	const auto realkeep = [&] {return keep && client.valid(); };

	f_send(); // activate

	while (realkeep())
	{
		std::shared_ptr<std::ifstream> iiii = ifptr;

		if (request_cancel)
		{
			f_send(package_commu::type::PACK_CANCEL_SEND);
			request_cancel = false;
		}
		else if (msg_usr.size())
		{
			std::lock_guard<std::mutex> luck(saf);

			package_commu muv = std::move(msg_usr.front());
			msg_usr.erase(msg_usr.begin());

			while (!client.send((char*)&muv, sizeof(muv)) && realkeep()) {
				cout_ctl(message_type::SYSTEM, "Bad socket.");
				std::this_thread::sleep_for(std::chrono::seconds(1));
			}
			if (!realkeep()) continue;
		}
		else if (ifptr_nam.size())
		{
			std::lock_guard<std::mutex> luck(saf);
			package_commu pkg;

			pkg.typ = package_commu::type::START_PACK;
			for (size_t p = 0; p < sizeof(pkg.raw.srt.file_name) && p < ifptr_nam.size(); ++p) pkg.raw.srt.file_name[p] = ifptr_nam[p];

			cout_ctl(message_type::SYSTEM, "About to send '" + ifptr_nam + "'...");

			ifptr_nam.clear();
			ifptr->seekg(0, std::ios::end);
			pkg.raw.srt.file_size = ifptr->tellg();
			ifptr->seekg(0, std::ios::beg);

			while (!client.send((char*)&pkg, sizeof(pkg)) && realkeep()) {
				cout_ctl(message_type::SYSTEM, "Bad socket.");
				std::this_thread::sleep_for(std::chrono::seconds(1));
			}
			if (!realkeep()) continue;
		}
		else if (iiii && iiii->eof())
		{
			std::lock_guard<std::mutex> luck(saf);
			package_commu pkg;

			pkg.typ = package_commu::type::END_PACK;

			cout_ctl(message_type::SYSTEM, "File sent completely.");

			ifptr.reset();

			while (!client.send((char*)&pkg, sizeof(pkg)) && realkeep()) {
				cout_ctl(message_type::SYSTEM, "Bad socket.");
				std::this_thread::sleep_for(std::chrono::seconds(1));
			}
			if (!realkeep()) continue;
		}
		else if (iiii)
		{
			package_commu pkg;

			pkg.typ = package_commu::type::PACK;

			iiii->read(pkg.raw.pck.data, sizeof(pkg.raw.pck.data));

			if (!iiii || iiii->gcount() == 0) {
				cout_ctl(message_type::SYSTEM, "Unexpected EOF or read error before eof()!");

				pkg.typ = package_commu::type::END_PACK;

				ifptr.reset();

				while (!client.send((char*)&pkg, sizeof(pkg)) && realkeep()) {
					cout_ctl(message_type::SYSTEM, "Bad socket.");
					std::this_thread::sleep_for(std::chrono::seconds(1));
				}
				if (!realkeep()) continue;
			}
			else {
				pkg.raw.pck.len = iiii->gcount();

				while (!client.send((char*)&pkg, sizeof(pkg)) && realkeep()) {
					cout_ctl(message_type::SYSTEM, "Bad socket.");
					std::this_thread::sleep_for(std::chrono::seconds(1));
				}
				if (!realkeep()) continue;
			}
		}
		else {
			package_commu pkg;

			pkg.typ = package_commu::type::SYNC;

			while (!client.send((char*)&pkg, sizeof(pkg)) && realkeep()) {
				cout_ctl(message_type::SYSTEM, "Bad socket.");
				std::this_thread::sleep_for(std::chrono::seconds(1));
			}
			if (!realkeep()) continue;
			std::this_thread::sleep_for(std::chrono::milliseconds(750));
		}
	}

	cout_ctl(message_type::SYSTEM, "Sending stuff disabled.");
}

void SyncedControl::async_recv()
{
	cout_ctl(message_type::SYSTEM, "Receiving stuff enabled.");

	const auto realkeep = [&] {return keep && client.valid(); };

	std::unique_ptr<std::ofstream> ofptr;

	while (realkeep())
	{
		package_commu pkg;

		{
			std::vector<char> rdat;
			while (rdat.size() < sizeof(pkg) && realkeep()) {
				auto _d = client.recv(sizeof(pkg) - rdat.size());
				rdat.insert(rdat.end(), _d.begin(), _d.end());
			}
			if (!realkeep()) continue;
			easy_move(pkg, rdat);
		}

		++pack_count;

		switch (pkg.typ)
		{
		case package_commu::type::SYNC:
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
			break;
		case package_commu::type::MESSAGE:
		{
			std::string ez(pkg.raw.msg.data, pkg.raw.msg.data + pkg.raw.msg.len);
			cout_ctl(message_type::MESSAGE, ez);
		}
		break;
		case package_commu::type::START_PACK:
		{
			std::string nam_fix(pkg.raw.srt.file_name);
			for (auto& ii : nam_fix) {
				if (!std::isalnum(ii) && ii != '.') ii = '_';
			}

			ofptr = std::make_unique<std::ofstream>(nam_fix, std::ios::binary | std::ios::out);
			if (!ofptr || !ofptr->good() || !ofptr->is_open()) {
				ofptr.reset();
				cout_ctl(message_type::SYSTEM, "Could not prepare download request for '" + nam_fix + "'.");
				request_cancel = true;
			}
			else {
				cout_ctl(message_type::SYSTEM, "Prepared for file download request '" + nam_fix + "' (size: " + std::to_string(pkg.raw.srt.file_size) + " byte(s)).");
				remaining = pkg.raw.srt.file_size;
			}
		}
		break;
		case package_commu::type::PACK:
		{
			if (!ofptr || !ofptr->good() || !ofptr->is_open() || remaining == 0) {
				ofptr.reset();
				cout_ctl(message_type::SYSTEM, "Still receiving data when it should not! Asking host nicely to STOP.");
				request_cancel = true;
			}
			else {
				if (pkg.raw.pck.len > remaining) {
					cout_ctl(message_type::SYSTEM, "Size doesn't match... Overflow on download?");
					remaining = 0;
				}
				else remaining -= pkg.raw.pck.len;

				ofptr->write(pkg.raw.pck.data, pkg.raw.pck.len);
			}
		}
		break;
		case package_commu::type::END_PACK:
		{
			if (!ofptr || !ofptr->good() || !ofptr->is_open()) {
				ofptr.reset();
				cout_ctl(message_type::SYSTEM, "END_PACK asked when there's no file to close.");
			}
			else {
				cout_ctl(message_type::SYSTEM, "Download complete.");
				ofptr->flush();
				ofptr.reset();
				if (remaining != 0) {
					cout_ctl(message_type::SYSTEM, "Closed file early? Remaining was not on 0. Closed anyway.");
					remaining = 0;
				}
			}
		}
		break;
		case package_commu::type::PACK_CANCEL_SEND:
		{
			cout_ctl(message_type::SYSTEM, "Other side cancelled download of file. Closed reading.");
			ifptr.reset();
			ifptr_nam.clear();
			remaining = 0;
		}
		break;
		}
	}

	cout_ctl(message_type::SYSTEM, "Receiving stuff disabled.");
}

SyncedControl::SyncedControl(Lunaris::TCP_client& a, const std::function<void(const message_type, const std::string&)>& b)
	: client(a), cout_ctl(b)
{
	if (client.empty() || !cout_ctl) throw std::invalid_argument("Null or empty arguments.");

	keep = true;
	thr_send = std::thread([this] { async_send(); });
	thr_recv = std::thread([this] { async_recv(); });
}

SyncedControl::~SyncedControl()
{
	keep = false;
	client.close_socket();
	if (thr_send.joinable()) thr_send.join();
	if (thr_recv.joinable()) thr_recv.join();
}

void SyncedControl::send_message(const std::string& s)
{
	std::lock_guard<std::mutex> luck(saf);
	package_commu pkg;
	pkg.typ = package_commu::type::MESSAGE;
	for (size_t p = 0; p < s.size() && p < sizeof(pkg.raw.msg.data); ++p) pkg.raw.msg.data[p] = s[p];
	pkg.raw.msg.len = s.size() > sizeof(pkg.raw.msg.data) ? sizeof(pkg.raw.msg.data) : s.size();
	msg_usr.push_back(std::move(pkg));
}

bool SyncedControl::send_file(const std::string& f)
{
	if (f.size() == 0 || ifptr) return false; // invalid or there's a file being sent already.

	std::shared_ptr<std::ifstream> snd = std::make_shared<std::ifstream>(f, std::ios::binary | std::ios::in);
	if (!snd || snd->bad() || !snd->is_open()) return false;

	std::lock_guard<std::mutex> luck(saf);
	{
		for (size_t pp = f.size() - 1; pp != 0; --pp)
		{
			if (f[pp] == '/' || f[pp] == '\\') {
				ifptr_nam = f.substr(pp);
			}
		}
		if (ifptr_nam.empty()) ifptr_nam = f;
	}
	ifptr = std::move(snd);
	return true;
}

uint64_t SyncedControl::get_pack_count() const
{
	return pack_count;
}

uint64_t SyncedControl::get_remaining() const
{
	return remaining;
}
