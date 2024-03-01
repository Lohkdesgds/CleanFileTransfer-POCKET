// Finished @ 2024/02/18 02:09

#pragma once

#include <AllegroCPP/include/file.h>
#include <AllegroCPP/include/thread.h>
#include <AllegroCPP/include/native_dialog.h>
#include <Lunaris/Encryption/encryption.h>
#include <Lunaris/EventPool/event_pool.h>
#include <atomic>

/*
How packets are made:
#SENDER
- One time first, sends key + mod (2 * uint64_t)
- Gets a fixed amount of data
- Generates a packet of { packet_size (encrypted) + encrypted_data }
#RECEIVER
- One time first, get key + mod (2 * uint64_t)
- Reads uint64_t with packet_size, then read packet_size of data
- Decrypt and go.
*/


/// <summary>
/// Syncronous read-encrypt-send/recv-decrypt-store class for data transfering
/// </summary>
class File_handler {
	static constexpr int64_t read_block_size = 2048;
	static constexpr uint64_t max_buffer_delay = 8192;
	static constexpr double max_buffer_delay_wait_factor = 0.8; // wait get down to this proportion of max_buffer_delay

	enum class e_type{SENDER, RECEIVER};
	enum class e_log_mode{DETAILED, RESUMED_PERCENTAGE};

	struct async_enc {
		std::vector<uint8_t> data;
		uint64_t seq;
	};

	AllegroCPP::File &m_wfp, &m_rfp;
	Lunaris::RSA_plus m_crypto;
	Lunaris::event_pool_async<async_enc> m_pool;
	AllegroCPP::Thread m_async; // optional
	std::atomic<uint64_t> m_seq_counter_sync = 0; // on async encrypt, do sequential write on File&
	uint64_t m_expected_total = 0;
	AllegroCPP::Text_log* m_ref_log = nullptr;
	const e_type m_type;
	e_log_mode m_log_mode = e_log_mode::RESUMED_PERCENTAGE;
	bool m_started = false, m_finished = false, m_is_overloaded = false;


	// m_pool.post call this
	void _async_enc(async_enc);

	// recv receives encrypted size, then reads its size, put on pool for decrypt and then write on final file in sequence
	void _sync_recv();
	// sender reads file and puts data to m_pool to then encrypt and send encrypted data size and data itself in sequence
	void _sync_send();

	friend File_handler make_encrypter(AllegroCPP::File& output, AllegroCPP::File& reading);
	friend File_handler make_decrypter(AllegroCPP::File& output, AllegroCPP::File& reading);
	friend File_handler* make_new_encrypter(AllegroCPP::File& output, AllegroCPP::File& reading);
	friend File_handler* make_new_decrypter(AllegroCPP::File& output, AllegroCPP::File& reading);

	// assist fcn for file write
	size_t c_write(void*, size_t);
	bool c_write(uint64_t);
	// assist fcn for file read
	size_t c_read(void*, size_t);
	bool c_read(uint64_t&);

	// auto lock thread if seq_now is > than m_seq_counter_sync + max_buffer_delay
	void load_balancer(const uint64_t seq_now);

	// lock thread until end
	void wait_end();

	File_handler(const e_type type, AllegroCPP::File& output, AllegroCPP::File& reading);
public:
	~File_handler();

	void start();
	void start_async();

	void link_logger(AllegroCPP::Text_log&, const e_log_mode = e_log_mode::RESUMED_PERCENTAGE);

	// [0..100]
	float get_progress() const;


	bool has_ended() const;
};


File_handler make_encrypter(AllegroCPP::File& output, AllegroCPP::File& reading);
File_handler make_decrypter(AllegroCPP::File& output, AllegroCPP::File& reading);
File_handler* make_new_encrypter(AllegroCPP::File& output, AllegroCPP::File& reading);
File_handler* make_new_decrypter(AllegroCPP::File& output, AllegroCPP::File& reading);

#ifdef _DEBUG
void file_reader_test();
#endif