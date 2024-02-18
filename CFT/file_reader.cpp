// Finished @ 2024/02/18 02:09

#include "file_reader.h"

#define LOG_AUTO(...) if (m_ref_log != nullptr) { *m_ref_log << __VA_ARGS__ << std::endl; }

// m_pool.post call this
void File_handler::_async_enc(async_enc dat)
{
	switch (m_type) {
	case e_type::SENDER:
	{
		// do heavy transform work (encrypt)
		m_crypto.transform(dat.data);
		// wait for its turn
		while (dat.seq != m_seq_counter_sync) std::this_thread::yield();

		if (m_log_mode == e_log_mode::DETAILED) LOG_AUTO("SENDER|POOL: Sending " << dat.seq << "!");

		// send things
		c_write(static_cast<uint64_t>(dat.data.size())); // #4...
		c_write(dat.data.data(), dat.data.size()); // #5...
		// tell next to go + check end
		m_finished = (++m_seq_counter_sync) == m_expected_total;
	}
	break;
	case e_type::RECEIVER:
	{
		// it is expected to get data of sent size, so no c_read here. heavy thing is transform.
		// decrypt
		m_crypto.transform(dat.data);
		// wait for its turn
		while (dat.seq != m_seq_counter_sync) std::this_thread::yield();

		if (m_log_mode == e_log_mode::DETAILED)LOG_AUTO("RECEIVER|POOL: Saving " << dat.seq << "!");

		// send things
		c_write(dat.data.data(), dat.data.size());
		// tell next to go + check end
		m_finished = (++m_seq_counter_sync) == m_expected_total;
	}
	break;
	}
}

void File_handler::_sync_recv()
{
	Lunaris::RSA_keys<uint64_t> keys{};

	LOG_AUTO("RECEIVER: Retrieving keys...");

	c_read(keys.key); // #1
	c_read(keys.mod); // #2

	LOG_AUTO("RECEIVER: Retrieving expected packets amount...");

	c_read(m_expected_total); // #3

	if (m_expected_total == 0) throw std::runtime_error("No data to expect? What?");

	m_crypto = Lunaris::make_decrypt_auto(keys);

	LOG_AUTO("RECEIVER: Start.");

	auto timed = std::chrono::system_clock().now();
	const auto start = timed;

	uint64_t seq = 0;

	for (uint64_t remaining = m_expected_total; remaining > 0;)
	{
		uint64_t packet_len = 0;
		c_read(packet_len); // #4

		if (packet_len == 0) throw std::runtime_error("Cannot get packet size for read :|");

		std::vector<uint8_t> buf(packet_len, '\0');
		c_read(buf.data(), packet_len); // #5

		--remaining; // packet

		switch (m_log_mode) {
		case e_log_mode::DETAILED:
			LOG_AUTO("RECEIVER: Posting " << seq << " to work.");
			// yes, no break
		case e_log_mode::RESUMED_PERCENTAGE:
			if (std::chrono::system_clock().now() - timed > std::chrono::seconds(1)) {
				timed = std::chrono::system_clock().now();
				LOG_AUTO("RECEIVER: Percentage: " << get_progress() << "%");
			}
			break;
		}

		m_pool.post(async_enc{ std::move(buf), seq++ });

		load_balancer(seq);
	}

	LOG_AUTO("RECEIVER: Waiting end...");

	wait_end();

	const auto taken = std::chrono::system_clock().now() - start;
	LOG_AUTO("RECEIVER: Took " << std::chrono::duration_cast<std::chrono::milliseconds>(taken).count() << " ms to complete task");

	LOG_AUTO("RECEIVER: End.");
}

void File_handler::_sync_send()
{
	m_crypto = Lunaris::make_encrypt_auto();
	const Lunaris::RSA_keys<uint64_t> keys = m_crypto.get_combo();
	
	const auto total_bytes = m_rfp.size();
	m_expected_total = (total_bytes / read_block_size) + (total_bytes % read_block_size > 0 ? 1 : 0);
	

	LOG_AUTO("SENDER: Sending keys...");

	c_write(keys.key); // #1
	c_write(keys.mod); // #2

	LOG_AUTO("SENDER: Sending packets amount...");

	c_write(static_cast<uint64_t>(m_expected_total)); // #3

	LOG_AUTO("SENDER: Start.");

	auto timed = std::chrono::system_clock().now();
	const auto start = timed;

	uint64_t seq = 0;

	for (uint64_t rem_bytes = total_bytes; rem_bytes > 0;)
	{
		const auto current_block_size = read_block_size < rem_bytes ? read_block_size : rem_bytes;
		std::vector<uint8_t> buf(current_block_size, '\0');
		
		const auto got_read = static_cast<uint64_t>(c_read(buf.data(), buf.size()));
		
		if (!got_read) continue;
		
		if (got_read != current_block_size) throw std::runtime_error("Broken read. Broken packet. Cannot continue.");
		
		rem_bytes -= got_read; // bytes

		switch (m_log_mode) {
		case e_log_mode::DETAILED:
			LOG_AUTO("SENDER: Posting " << seq << " to work.");
			// yes, no break
		case e_log_mode::RESUMED_PERCENTAGE:
			if (std::chrono::system_clock().now() - timed > std::chrono::seconds(1)) {
				timed = std::chrono::system_clock().now();
				LOG_AUTO("SENDER: Percentage: " << get_progress() << "%");
			}
			break;
		}		

		m_pool.post(async_enc{ std::move(buf), seq++ });

		load_balancer(seq);
	}

	LOG_AUTO("SENDER: Waiting end...");

	wait_end();

	const auto taken = std::chrono::system_clock().now() - start;
	LOG_AUTO("SENDER: Took " << std::chrono::duration_cast<std::chrono::milliseconds>(taken).count() << " ms to complete task");

	LOG_AUTO("SENDER: End.");
}

size_t File_handler::c_write(void* d, size_t l)
{
	return m_wfp.write(d, l);
}

bool File_handler::c_write(uint64_t u)
{
	return c_write((char*)&u, sizeof(u)) == sizeof(u);
}

size_t File_handler::c_read(void* d, size_t l)
{
	size_t gottn = 0;
	while (gottn < l && !m_wfp.eof()) {
		gottn += m_rfp.read((char*)d + gottn, l - gottn);
	}
	return gottn;
}

bool File_handler::c_read(uint64_t& u)
{
	return c_read((char*)&u, sizeof(u)) == sizeof(u);
}

void File_handler::load_balancer(const uint64_t seq_now)
{
	if (seq_now - m_seq_counter_sync > max_buffer_delay) {
		while (m_is_overloaded = (seq_now > (m_seq_counter_sync + static_cast<uint64_t>(max_buffer_delay * 0.8)))) {
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
		}
	}
}

void File_handler::wait_end()
{
	while (!has_ended()) {
		std::this_thread::yield();
		std::this_thread::sleep_for(std::chrono::milliseconds(50));
	}
}

File_handler::File_handler(const e_type type, AllegroCPP::File& output, AllegroCPP::File& reading)
	: m_wfp(output), m_rfp(reading), m_pool([this](auto&& d) {this->_async_enc(std::move(d)); }), m_type(type)
{
}

File_handler::~File_handler()
{
	wait_end();
}

void File_handler::start()
{
	if (m_started) return;
	m_started = true;

	switch (m_type) {
	case e_type::SENDER:
		_sync_send();
		break;
	case e_type::RECEIVER:
		_sync_recv();
		break;
	}
}

void File_handler::start_async()
{
	if (m_started) return;
	m_started = true;

	switch (m_type) {
	case e_type::SENDER:
		m_async = AllegroCPP::Thread([this] { _sync_send(); return false; });
		break;
	case e_type::RECEIVER:
		m_async = AllegroCPP::Thread([this] { _sync_recv(); return false; });
		break;
	}	
}

void File_handler::link_logger(AllegroCPP::Text_log& l, const e_log_mode lm)
{
	m_ref_log = &l;
	m_log_mode = lm;
}

float File_handler::get_progress() const
{
	return m_expected_total == 0 ? -1.0f : static_cast<float>(100.0 * m_seq_counter_sync / m_expected_total);
}

bool File_handler::has_ended() const
{
	return m_finished;
}

File_handler make_encrypter(AllegroCPP::File& output, AllegroCPP::File& reading)
{
	return File_handler(File_handler::e_type::SENDER, output, reading);
}

File_handler make_decrypter(AllegroCPP::File& output, AllegroCPP::File& reading)
{
	return File_handler(File_handler::e_type::RECEIVER, output, reading);
}

#ifdef _DEBUG
void file_reader_test()
{
	printf_s("[file_reader_test] Generating content for test...\n");

	constexpr size_t exact_string_size = 25600328;

	// Prepare terrain
	char* src_buf = new char[exact_string_size] {};
	char* aft_buf = nullptr;

	const char first_msg[] = "This is an example of data. As the test needs some random stuff to work, we'll fill this buffer with 0 to 255 information for a while, encrypt, decrypt and check if data is still the same.\n";
	const char secon_msg[] = "\nNow, we prepare our terrain. Create a file, then read and create another one with encrypted data, then decrypt on a third, then read back.";

	memcpy(src_buf, first_msg, (std::size(first_msg) - 1));
	memcpy(src_buf + exact_string_size - (std::size(secon_msg) - 1), secon_msg, (std::size(secon_msg) - 1));

	for (uint32_t times = 0; times < 100000; ++times) { // around 25.6 MB
		for (int16_t to_char = -128; to_char < 128; ++to_char) {
			//source_data.insert(source_data.end(), (char)to_char);
			const char ch = static_cast<char>(to_char);
			memcpy(src_buf + (std::size(first_msg) - 1) + (static_cast<uint64_t>(to_char + 128) + static_cast<uint64_t>(256) * static_cast<uint64_t>(times)), &ch, 1);
		}
	}

	//source_data += ;

	printf_s("[file_reader_test] Flushing to a file...\n");
	// fill up test file
	{
		AllegroCPP::File fp("test_source.txt", "wb");
		fp.write(src_buf, exact_string_size);
	}

	printf_s("[file_reader_test] Encrypting from a file to another...\n");
	// Encrypt phase
	{
		AllegroCPP::File read_from("test_source.txt", "rb");
		AllegroCPP::File write_to("test_encrypted.txt", "wb");

		auto wrk = make_encrypter(write_to, read_from);
		wrk.start();
	}

	printf_s("[file_reader_test] Decrypting from a file to another...\n");
	// Decrypt phase
	{
		AllegroCPP::File read_from("test_encrypted.txt", "rb");
		AllegroCPP::File write_to("test_decrypted.txt", "wb");

		auto wrk = make_decrypter(write_to, read_from);
		wrk.start();
	}

	printf_s("[file_reader_test] Loading decrypted data into memory for comparison...\n");
	// Load in memory
	{
		AllegroCPP::File fp("test_decrypted.txt", "rb");
		aft_buf = new char[exact_string_size] {};

		
		fp.read(aft_buf, exact_string_size);

		printf_s("[file_reader_test] Testing...\n");
		//if (!fp.eof()) throw std::runtime_error("After read back, file is not at the end after expected max read.");
		if (memcmp(src_buf, aft_buf, exact_string_size) != 0) throw std::runtime_error("Data mismatch!");
	}

	printf_s("[file_reader_test] Test passed. Removing files and finishing...\n");

	// Passed the test
	delete[] src_buf;
	delete[] aft_buf;

	std::remove("test_source.txt");
	std::remove("test_encrypted.txt");
	std::remove("test_decrypted.txt");

	printf_s("[file_reader_test] Test ended.\n");
}
#endif