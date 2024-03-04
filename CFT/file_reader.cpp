// Finished @ 2024/02/18 02:09

#include "file_reader.h"

#define LOG_AUTO(...) if (m_ref_log != nullptr) { *m_ref_log << __VA_ARGS__ << std::endl; }

// m_pool.post call this
void File_handler::_async_enc(async_enc dat)
{
	try {
		switch (m_type) {
		case e_type::SENDER:
		{
			// do heavy transform work (encrypt)
			m_crypto.transform(dat.data);
			// wait for its turn
			while (dat.seq != m_seq_counter_sync) {
				std::this_thread::yield();
				if (m_abort) return;
			}

			if (m_log_mode == e_log_mode::DETAILED) LOG_AUTO("SENDER|POOL: Sending " << dat.seq << "!");

			// send things
			if (!c_write(static_cast<uint64_t>(dat.data.size()))) throw std::runtime_error("Disconnected or cannot write!"); // #4...
			if (!c_write(dat.data.data(), dat.data.size())) throw std::runtime_error("Disconnected or cannot write!"); // #5...
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
			while (dat.seq != m_seq_counter_sync) {
				std::this_thread::yield();
				if (m_abort) return;
			}

			if (m_log_mode == e_log_mode::DETAILED)LOG_AUTO("RECEIVER|POOL: Saving " << dat.seq << "!");

			// send things
			if (!c_write(dat.data.data(), dat.data.size())) throw std::runtime_error("Disconnected or cannot write!");
			// tell next to go + check end
			m_finished = (++m_seq_counter_sync) == m_expected_total;
		}
		break;
		}
	}
	catch (...) {
		m_exceptions.push_back(std::current_exception());
		LOG_AUTO((m_type == e_type::SENDER ? "SENDER" : "RECEIVER") << ": Got exception. Aborting...");
		m_abort = true;
	}
}

void File_handler::_sync_recv()
{
	try {
		Lunaris::RSA_keys<uint64_t> keys{};

		LOG_AUTO("RECEIVER: Retrieving keys...");

		if (!c_read(keys.key)) throw std::runtime_error("Disconnected or cannot read!"); // #1
		if (!c_read(keys.mod)) throw std::runtime_error("Disconnected or cannot read!"); // #2

		LOG_AUTO("RECEIVER: Retrieving expected packets amount...");

		if (!c_read(m_expected_total)) throw std::runtime_error("Disconnected or cannot read!"); // #3

		if (m_expected_total == 0) throw std::runtime_error("No data to expect? What?");

		m_crypto = Lunaris::make_decrypt_auto(keys);

		LOG_AUTO("RECEIVER: Start.");

		auto timed = std::chrono::system_clock().now();
		const auto start = timed;

		uint64_t seq = 0;

		for (uint64_t remaining = m_expected_total; remaining > 0;)
		{
			if (m_abort) return;

			uint64_t packet_len = 0;
			c_read(packet_len); // #4

			if (packet_len == 0) throw std::runtime_error("Cannot get packet size for read :|");

			std::vector<uint8_t> buf(packet_len, '\0');
			if (!c_read(buf.data(), packet_len)) throw std::runtime_error("Disconnected or cannot read!"); // #5

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
	catch (...) {
		m_exceptions.push_back(std::current_exception());
		LOG_AUTO("RECEIVER: Got exception. Aborting...");
		m_abort = true;
	}
}

void File_handler::_sync_send()
{
	try {
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
			if (m_abort) return;

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
	catch (...) {
		m_exceptions.push_back(std::current_exception());
		LOG_AUTO("SENDER: Got exception. Aborting...");
		m_abort = true;
	}
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
	size_t fails = 0;
	while (gottn < l && !m_wfp.eof()) {
		const auto cnt = m_rfp.read((char*)d + gottn, l - gottn);
		if (cnt == 0) {
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
			if (++fails > 50) return gottn;
		}
		else fails = 0;
		gottn += cnt;
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
			if (m_abort) return;
		}
	}
}

void File_handler::wait_end()
{
	while (!has_ended()) {
		std::this_thread::yield();
		std::this_thread::sleep_for(std::chrono::milliseconds(50));
		if (m_abort) return;
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

void File_handler::abort()
{
	m_abort = true;
	m_async.stop();
	m_async.join();
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

bool File_handler::has_exceptions() const
{
	return m_exceptions.size();
}

void File_handler::rethrow_next_exception()
{
	if (m_exceptions.size() == 0) return;
	std::exception_ptr i = *m_exceptions.begin();
	m_exceptions.erase(m_exceptions.begin());
	std::rethrow_exception(i);
}

bool File_handler::has_ended() const
{
	return m_finished || m_abort || m_exceptions.size() > 0;
}

File_handler make_encrypter(AllegroCPP::File& output, AllegroCPP::File& reading)
{
	return File_handler(File_handler::e_type::SENDER, output, reading);
}

File_handler make_decrypter(AllegroCPP::File& output, AllegroCPP::File& reading)
{
	return File_handler(File_handler::e_type::RECEIVER, output, reading);
}

File_handler* make_new_encrypter(AllegroCPP::File& output, AllegroCPP::File& reading)
{
	return new File_handler(File_handler::e_type::SENDER, output, reading);
}

File_handler* make_new_decrypter(AllegroCPP::File& output, AllegroCPP::File& reading)
{
	return new File_handler(File_handler::e_type::RECEIVER, output, reading);
}

#ifdef _DEBUG
void file_reader_test()
{	
	printf_s("[file_reader_test] Preparing test ground...\n");

	constexpr u_short port_used_in_tcp_test = 25566;
	constexpr size_t exact_string_size = 25600328;
	const char first_msg[] = "This is an example of data. As the test needs some random stuff to work, we'll fill this buffer with 0 to 255 information for a while, encrypt, decrypt and check if data is still the same.\n";
	const char secon_msg[] = "\nNow, we prepare our terrain. Create a file, then read and create another one with encrypted data, then decrypt on a third, then read back.";
	const char file_names[4][36] = { "test_source.txt", "test_encrypted.txt", "test_decrypted.txt", "partial_decr_stop.txt"};

	// Prepare terrain
	char* src_buf = new char[exact_string_size] {};
	char* aft_buf = new char[exact_string_size] {};

	const auto reset_bufs = [&] {
		memset(src_buf, 0, exact_string_size);
		memset(aft_buf, 0, exact_string_size);

		memcpy(src_buf, first_msg, (std::size(first_msg) - 1));
		memcpy(src_buf + exact_string_size - (std::size(secon_msg) - 1), secon_msg, (std::size(secon_msg) - 1));

		for (uint32_t times = 0; times < 100000; ++times) { // around 25.6 MB
			for (int16_t to_char = -128; to_char < 128; ++to_char) {
				//source_data.insert(source_data.end(), (char)to_char);
				const char ch = static_cast<char>(to_char);
				memcpy(src_buf + (std::size(first_msg) - 1) + (static_cast<uint64_t>(to_char + 128) + static_cast<uint64_t>(256) * static_cast<uint64_t>(times)), &ch, 1);
			}
		}
	};

	const auto test_final_to_mem = [&] {
		printf_s("[file_reader_test] Loading decrypted data into memory for comparison...\n");
		// Load in memory
		{
			AllegroCPP::File fp(file_names[2], "rb");

			fp.read(aft_buf, exact_string_size);

			printf_s("[file_reader_test] Testing...\n");
			//if (!fp.eof()) throw std::runtime_error("After read back, file is not at the end after expected max read.");
			if (memcmp(src_buf, aft_buf, exact_string_size) != 0) throw std::runtime_error("Data mismatch!");
		}
		printf_s("[file_reader_test] Test good!\n");
	};


	printf_s("[file_reader_test] Generating content for test...\n");
	reset_bufs();


	printf_s("[file_reader_test] Flushing generated data to source file...\n");
	// fill up test file
	{
		AllegroCPP::File fp(file_names[0], "wb");
		fp.write(src_buf, exact_string_size);
	}

	printf_s("[file_reader_test] Starting tests using this generated file...\n");
	
	// #=#=#=#=#=#=#=#=#=#=#=#=#=#=# FIRST STEP: FILE 2 FILE 2 FILE #=#=#=#=#=#=#=#=#=#=#=#=#=#=# //
	
	printf_s("[file_reader_test] ### DISK FILE TEST ONGOING ###\n");
	
	printf_s("[file_reader_test] Encrypting from a file to another...\n");
	// Encrypt phase
	{
		AllegroCPP::Text_log tl("FRT: Disk file encryption");
		AllegroCPP::File read_from(file_names[0], "rb");
		AllegroCPP::File write_to(file_names[1], "wb");
	
		auto wrk = make_encrypter(write_to, read_from);
		wrk.link_logger(tl);
		wrk.start();
	}
	
	printf_s("[file_reader_test] Decrypting from a file to another...\n");
	// Decrypt phase
	{
		AllegroCPP::Text_log tl("FRT: Disk file decryption");
		AllegroCPP::File read_from(file_names[1], "rb");
		AllegroCPP::File write_to(file_names[2], "wb");
	
		auto wrk = make_decrypter(write_to, read_from);
		wrk.link_logger(tl);
		wrk.start();
	}
	
	test_final_to_mem();
	
	printf_s("[file_reader_test] ### DISK FILE TEST OK!!! ###\n");
	
	// clean up last test, keep source
	std::remove(file_names[1]);
	std::remove(file_names[2]);
	
	// #=#=#=#=#=#=#=#=#=#=#=#=#=#=# SECOND STEP: FILE 2 FILE 2 FILE #=#=#=#=#=#=#=#=#=#=#=#=#=#=# //
	
	printf_s("[file_reader_test] ### SOCKET TRANSFER TEST ONGOING ###\n");
	
	printf_s("[file_reader_test] Launching two asynchronous threads to simulate a double-pc read/write socket scenario...\n");
	
	{
		AllegroCPP::Text_log tl("FRT: Async encrypter/decrypter");
	
		std::atomic<int> step[2]; // 0 = sender, 1 = recv. Steps: { INIT = 0, READY = 1, DOING = 2, END = 3 }
	
		AllegroCPP::Thread sender_thr([&] {
			auto& stat = step[0];
	
			stat = 0;
			AllegroCPP::File file_source(file_names[0], "rb"); // original
			AllegroCPP::File_host host(port_used_in_tcp_test);
	
			printf_s("[file_reader_test] THREAD#1: Waiting client to connect now.\n");
			stat = 1;
	
			auto file_write = host.listen(30000);
			if (file_write.empty()) throw std::runtime_error("FATAL ERROR: Client never came to test, so failure!");
	
			auto wrk = make_encrypter(file_write, file_source);
			wrk.link_logger(tl);
	
			stat = 2;
			wrk.start();
	
			stat = 3;
			return false;
		});
	
		while (step[0] != 1) std::this_thread::sleep_for(std::chrono::milliseconds(10));
	
		AllegroCPP::Thread recvr_thr([&] {
			auto& stat = step[0];
	
			stat = 0;
			AllegroCPP::File file_destiny(file_names[2], "wb"); // final
			AllegroCPP::File_client file_client("localhost", port_used_in_tcp_test);
	
			if (!file_client || file_client.has_error()) throw std::runtime_error("FATAL ERROR: Cannot create client to connect to host test");
	
			printf_s("[file_reader_test] THREAD#2: Connected to host. Begin.\n");
			stat = 1;
	
			auto wrk = make_decrypter(file_destiny, file_client);
			wrk.link_logger(tl);
	
			stat = 2;
			wrk.start();
	
			stat = 3;
			return false;
		});
	
		printf_s("[file_reader_test] Waiting for the end...\n");
		while (step[0] != 3 && step[1] != 3) std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}
	
	printf_s("[file_reader_test] ### SOCKET TRANSFER TEST OK!!! ###\n");
	
	std::remove(file_names[2]);

	// #=#=#=#=#=#=#=#=#=#=#=#=#=#=# THIRD STEP: FILE 2 FILE 2 FILE #=#=#=#=#=#=#=#=#=#=#=#=#=#=# //

	printf_s("[file_reader_test] ### SOCKET TRANSFER WITH MIDDLE STOP ONGOING + ASYNC ###\n");

	printf_s("[file_reader_test] Launching two asynchronous threads to simulate a double-pc read/write socket scenario...\n");

	{
		AllegroCPP::Text_log tl("FRT: Async encrypter/decrypter");
		std::atomic<int> step[2]; // 0 = sender, 1 = recv. Steps: { INIT = 0, READY = 1, DOING = 2, END = 3 }

		AllegroCPP::Thread sender_thr([&] {
			auto& stat = step[0];

			stat = 0;
			AllegroCPP::File file_source(file_names[0], "rb"); // original
			AllegroCPP::File_host host(port_used_in_tcp_test);

			printf_s("[file_reader_test] THREAD#1: Waiting client to connect now.\n");
			stat = 1;

			auto file_write = host.listen(30000);
			if (file_write.empty()) throw std::runtime_error("FATAL ERROR: Client never came to test, so failure!");

			auto wrk = make_encrypter(file_write, file_source);
			wrk.link_logger(tl/*, File_handler::e_log_mode::DETAILED*/);

			stat = 2;
			wrk.start_async();

			while (!wrk.has_ended()) {
				if (wrk.get_progress() >= 30.0) {
					printf_s("[file_reader_test] THREAD#1: Testing abort NOW!.\n");
					wrk.abort();
					printf_s("[file_reader_test] THREAD#1: BREAKING AND LEAVING\n");
					break;
				}
				std::this_thread::sleep_for(std::chrono::milliseconds(50));
			}

			stat = 3;
			return false;
		});

		while (step[0] != 1) std::this_thread::sleep_for(std::chrono::milliseconds(10));

		AllegroCPP::Thread recvr_thr([&] {
			auto& stat = step[0];

			stat = 0;
			AllegroCPP::File file_destiny(file_names[2], "wb"); // final
			AllegroCPP::File_client file_client("localhost", port_used_in_tcp_test);

			if (!file_client || file_client.has_error()) throw std::runtime_error("FATAL ERROR: Cannot create client to connect to host test");

			printf_s("[file_reader_test] THREAD#2: Connected to host. Begin.\n");
			stat = 1;

			auto wrk = make_decrypter(file_destiny, file_client);
			wrk.link_logger(tl/*, File_handler::e_log_mode::DETAILED*/);

			stat = 2;
			wrk.start_async();

			while(!wrk.has_ended())
				std::this_thread::sleep_for(std::chrono::milliseconds(100));


			stat = 3;
			return false;
			});

		printf_s("[file_reader_test] Waiting for the end...\n");
		while (step[0] != 3 && step[1] != 3) std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}

	printf_s("[file_reader_test] ### SOCKET TRANSFER WITH MIDDLE STOP ONGOING + ASYNC TEST OK!!! ###\n");

	std::remove(file_names[2]);

	// #=#=#=#=# END

	printf_s("[file_reader_test] Tests passed. Cleanup...\n");

	// Passed the test
	delete[] src_buf;
	delete[] aft_buf;

	// ensure all
	std::remove(file_names[0]);
	std::remove(file_names[1]);
	std::remove(file_names[2]);

	printf_s("[file_reader_test] Test ended.\n");
}
#endif