#pragma once

#include <Socket/socket.h>
#include <Console/console.h>

#include <thread>
#include <mutex>
#include <vector>
#include <functional>
#include <fstream>
#include <string>

constexpr uint64_t package_len = 1 << 10;

constexpr uint64_t filename_len = 1 << 8;
constexpr uint64_t file_data_len = package_len - sizeof(uint64_t);

struct package_commu {
	enum class type : uint32_t { SYNC, PACK_CANCEL_SEND, START_PACK, PACK, END_PACK, MESSAGE };

	type typ = type::SYNC;
	union {
		struct {
			char file_name[filename_len];
			uint64_t file_size;
		} srt; // start_pack
		struct {
			char data[file_data_len];
			uint64_t len;
		} pck; // pack
		struct {
			char data[file_data_len];
			uint64_t len;
		} msg; // message
	} raw{};
};

template<typename T, typename V> void easy_move(T& dst, V& vec) { if (sizeof(dst) != vec.size()) throw std::runtime_error("Size mismatch"); std::copy(vec.begin(), vec.end(), (char*)&dst); }

class SyncedControl {
public:
	enum class message_type {MESSAGE, SYSTEM};
private:
	Lunaris::TCP_client& client;
	std::thread thr_send, thr_recv;

	std::shared_ptr<std::ifstream> ifptr;
	std::string ifptr_nam;
	std::vector<package_commu> msg_usr;
	std::mutex saf;

	bool keep = false;
	uint64_t remaining = 0;
	uint64_t pack_count = 0;

	// between threads
	bool request_cancel = false;

	const std::function<void(const message_type, const std::string&)> cout_ctl;

	void async_send();
	void async_recv();
public:
	SyncedControl(Lunaris::TCP_client&, const std::function<void(const message_type, const std::string&)>&);
	~SyncedControl();

	void send_message(const std::string&);
	bool send_file(const std::string&);

	uint64_t get_pack_count() const;
	uint64_t get_remaining() const;
};