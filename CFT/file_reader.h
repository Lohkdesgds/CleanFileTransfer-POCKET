#pragma once

#include <AllegroCPP/include/file.h>
#include <AllegroCPP/include/thread.h>
#include <AllegroCPP/include/native_dialog.h>
#include <Lunaris/Encryption/encryption.h>
#include <Lunaris/EventPool/event_pool.h>
#include <atomic>

constexpr int64_t file_read_blocks_of = 2048;
constexpr int64_t file_read_max_pending = 128;


class block_encrypted_info_begin {
	std::vector<uint8_t> m_buf;
public:
	block_encrypted_info_begin(const Lunaris::RSA_keys<uint64_t>&);
	// must be uint64_t * 2 in size
	block_encrypted_info_begin(std::vector<uint8_t>&&);

	uint64_t get_key() const;
	uint64_t get_mod() const;

	// always full data as vector.
	const std::vector<uint8_t>& get_all_data() const;
};
class block_encrypted {
	std::vector<uint8_t> m_buf;

	uint64_t m_order = 0, m_length = 0;
public:
	block_encrypted(const uint64_t order, std::vector<uint8_t>&& data, const Lunaris::RSA_plus& encryption_device);
	// must be min uint64_t * 2 in size
	block_encrypted(std::vector<uint8_t>&& data, const Lunaris::RSA_plus& decryption_device);

	uint64_t get_order() const;
	uint64_t get_length() const;

	// when called from order, data, encrypted_device, data to send. else data itself (get order and length from methods)
	const std::vector<uint8_t>& get_all_data() const;
};

// from raw not encoded to encoded. CPU intensive.
block_encrypted encrypt_from_raw(std::vector<uint8_t>&& data, const uint64_t offset, const Lunaris::RSA_plus& encryption_device);

// from encoded to raw
block_encrypted decrypt_from_raw(std::vector<uint8_t>&& data, const Lunaris::RSA_plus& decryption_device);



struct file_reader_params {
	std::vector<uint8_t> data;
	uint64_t offset;
	const Lunaris::RSA_plus* encrypter;

	file_reader_params(std::vector<uint8_t>&& data, const uint64_t offset, const Lunaris::RSA_plus& encryption_device) noexcept;
	file_reader_params(const file_reader_params&) = delete;
	file_reader_params(file_reader_params&&) noexcept;
	void operator=(const file_reader_params&) = delete;
	void operator=(file_reader_params&&) noexcept;
};

struct encrypted_data_post {
	std::vector<uint8_t>&& encrypted_data;
};


class File_reader_encrypter {
	Lunaris::RSA_plus m_encrypter = Lunaris::make_encrypt_auto();

	std::atomic<uint64_t> m_offset_counter = 0; // before post, read count
	std::atomic<uint64_t> m_offset_post_counter = 0; // after post, after encrypt and call

	AllegroCPP::Thread m_read_thr;
	Lunaris::event_pool_async<file_reader_params> m_pool;
	double m_progress = 0.0; // 0..100

	std::mutex m_output_mtx;
	AllegroCPP::File& m_output;
	AllegroCPP::File& m_input;

	AllegroCPP::Text_log* m_logging = nullptr;
	AllegroCPP::Thread m_logging_keep_up_to_date;
	bool m_logging_keep_up = true;

	// debug
	bool is_overloaded = false;
	bool has_ended_read = false;

	void debug_log(const std::string&);

	void async_read();
	void async_parallel_encrypt(file_reader_params&&);

	bool async_logging();
public:
	File_reader_encrypter(AllegroCPP::File& out, AllegroCPP::File& inn, const bool log_it = false);

	File_reader_encrypter(const File_reader_encrypter&) = delete;
	File_reader_encrypter(File_reader_encrypter&&) = delete;
	void operator=(const File_reader_encrypter&) = delete;
	void operator=(File_reader_encrypter&&) = delete;

	~File_reader_encrypter();

	bool is_working() const;
	uint64_t load_amount() const;
};