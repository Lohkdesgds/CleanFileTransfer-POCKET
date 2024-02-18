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

	friend File_handler make_sender(AllegroCPP::File& output, AllegroCPP::File& reading);
	friend File_handler make_receiver(AllegroCPP::File& output, AllegroCPP::File& reading);

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


File_handler make_sender(AllegroCPP::File& output, AllegroCPP::File& reading);
File_handler make_receiver(AllegroCPP::File& output, AllegroCPP::File& reading);



//constexpr int64_t file_read_blocks_of = 2048;
//constexpr int64_t file_read_max_pending = 16384;
//
//class NoCopyMove {
//public:
//	NoCopyMove() {}
//	NoCopyMove(const NoCopyMove&) = delete;
//	NoCopyMove(NoCopyMove&&) = delete;
//	void operator=(const NoCopyMove&) = delete;
//	void operator=(NoCopyMove&&) = delete;
//};
//
//class Loggable : public NoCopyMove {
//	AllegroCPP::Text_log* m_logging = nullptr;
//protected:
//	Loggable(const bool start_it);
//	~Loggable();
//
//	void debug_log(const std::string&);
//};
//
//
//class block_encrypted_info_begin {
//	std::vector<uint8_t> m_buf;
//public:
//	static constexpr uint64_t data_length = sizeof(uint64_t) * 2;
//
//	block_encrypted_info_begin(const Lunaris::RSA_keys<uint64_t>&);
//	// must be uint64_t * 2 in size
//	block_encrypted_info_begin(std::vector<uint8_t>&&);
//
//	uint64_t get_key() const;
//	uint64_t get_mod() const;
//
//	// always full data as vector.
//	const std::vector<uint8_t>& get_all_data() const;
//};
//class block_encrypted {
//	std::vector<uint8_t> m_buf;
//
//	uint64_t m_order = 0, m_length = 0;
//public:
//	block_encrypted(const uint64_t order, std::vector<uint8_t>&& data, const Lunaris::RSA_plus& encryption_device);
//	// must be min uint64_t * 2 in size
//	block_encrypted(std::vector<uint8_t>&& data, const Lunaris::RSA_plus& decryption_device);
//
//	uint64_t get_order() const;
//	uint64_t get_length() const;
//
//	// when called from order, data, encrypted_device, data to send. else data itself (get order and length from methods)
//	const std::vector<uint8_t>& get_all_data() const;
//};
//
//// from raw not encoded to encoded. CPU intensive.
//block_encrypted encrypt_from_raw(std::vector<uint8_t>&& data, const uint64_t offset, const Lunaris::RSA_plus& encryption_device);
//
//// from encoded to raw
//block_encrypted decrypt_from_raw(std::vector<uint8_t>&& data, const Lunaris::RSA_plus& decryption_device);
//
//
//
//struct file_reader_params {
//	std::vector<uint8_t> data;
//	uint64_t offset;
//	const Lunaris::RSA_plus* encrypter;
//
//	file_reader_params(std::vector<uint8_t>&& data, const uint64_t offset, const Lunaris::RSA_plus& encryption_device) noexcept;
//	file_reader_params(const file_reader_params&) = delete;
//	file_reader_params(file_reader_params&&) noexcept;
//	void operator=(const file_reader_params&) = delete;
//	void operator=(file_reader_params&&) noexcept;
//};
//
//struct encrypted_data_post {
//	std::vector<uint8_t>&& encrypted_data;
//};
//
//
//// The one that gets the raw file and sends it encrypted (inn -> encrypt -> out)
//class File_transfer_worker : Loggable {
//public:
//	enum class type { SENDER, RECEIVER };
//private:
//	const type m_self_type;
//
//	Lunaris::RSA_plus m_encrypter;
//
//	std::atomic<uint64_t> m_offset_counter = 0; // before post, read count
//	std::atomic<uint64_t> m_offset_post_counter = 0; // after post, after encrypt and call
//
//	AllegroCPP::Thread m_read_thr;
//	Lunaris::event_pool_async<file_reader_params> m_pool;
//	double m_progress = 0.0; // 0..100
//
//	std::mutex m_output_mtx;
//	AllegroCPP::File& m_output;
//	AllegroCPP::File& m_input;
//
//	AllegroCPP::Thread m_logging_keep_up_to_date;
//	bool m_logging_keep_up = true;
//
//	// debug
//	bool is_overloaded = false;
//	bool has_ended_read = false;
//
//
//	void async_read();
//	void async_parallel_criptography(file_reader_params&&);
//
//	bool async_logging();
//public:
//	File_transfer_worker(const type typo, AllegroCPP::File& out, AllegroCPP::File& inn, const bool log_it = false);
//	~File_transfer_worker();
//
//	bool is_working() const;
//	uint64_t load_amount() const;
//};
