#include "file_reader.h"

#define LOG_AUTO(...) if (m_ref_log != nullptr) { *m_ref_log << __VA_ARGS__ << std::endl; }

File_handler make_sender(AllegroCPP::File& output, AllegroCPP::File& reading)
{
	return File_handler(File_handler::e_type::SENDER, output, reading);
}

File_handler make_receiver(AllegroCPP::File& output, AllegroCPP::File& reading)
{
	return File_handler(File_handler::e_type::RECEIVER, output, reading);
}


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



//Loggable::Loggable(const bool start_it)
//	: m_logging(start_it ? (new AllegroCPP::Text_log("File reader encrypter")) : nullptr)
//{
//}
//
//Loggable::~Loggable()
//{
//	if (m_logging) delete m_logging;
//	m_logging = nullptr;
//}
//
//void Loggable::debug_log(const std::string& s)
//{
//	if (m_logging) *m_logging << s << std::endl;
//}
//
//block_encrypted_info_begin::block_encrypted_info_begin(const Lunaris::RSA_keys<uint64_t>& k)
//{
//	m_buf.resize(data_length);
//	uint64_t* _key = (uint64_t*)m_buf.data();
//	uint64_t* _mod = (uint64_t*)m_buf.data() + 1;
//	*_key = k.key;
//	*_mod = k.mod;
//}
//
//block_encrypted_info_begin::block_encrypted_info_begin(std::vector<uint8_t>&& data)
//	: m_buf(std::move(data))
//{
//	if (m_buf.size() != data_length) throw std::invalid_argument("Expected 2 uint64_t size, got different @ block_encrypted_info_begin");
//}
//
//uint64_t block_encrypted_info_begin::get_key() const
//{
//	uint64_t* _key = (uint64_t*)m_buf.data();
//	return *_key;
//}
//
//uint64_t block_encrypted_info_begin::get_mod() const
//{
//	uint64_t* _mod = (uint64_t*)m_buf.data() + 1;
//	return *_mod;
//}
//
//const std::vector<uint8_t>& block_encrypted_info_begin::get_all_data() const
//{
//	return m_buf;
//}
//
//block_encrypted::block_encrypted(const uint64_t order, std::vector<uint8_t>&& data, const Lunaris::RSA_plus& encryption_device)
//{
//	std::vector<uint8_t> to_encode = std::move(data);
//	encryption_device.transform(to_encode);
//
//	m_buf.resize(sizeof(uint64_t) * 2);
//
//	uint64_t* _order = (uint64_t*)m_buf.data();
//	uint64_t* _length = (uint64_t*)m_buf.data() + 1;
//
//	m_order = *_order = order;
//	m_length = *_length = static_cast<uint64_t>(to_encode.size());
//
//	m_buf.insert(m_buf.end(), std::move_iterator(to_encode.begin()), std::move_iterator(to_encode.end()));
//	to_encode.clear();
//}
//
//block_encrypted::block_encrypted(std::vector<uint8_t>&& data, const Lunaris::RSA_plus& decryption_device)
//{
//	std::vector<uint8_t> to_decode = std::move(data);
//
//	m_order = *(uint64_t*)to_decode.data();
//	m_length = *((uint64_t*)to_decode.data()) + 1;
//
//	to_decode.erase(to_decode.begin(), to_decode.begin() + (sizeof(uint64_t) / sizeof(uint8_t)) * 2);
//
//	decryption_device.transform(to_decode);
//
//	m_buf = std::move(to_decode);
//}
//
//uint64_t block_encrypted::get_order() const
//{
//	return m_order;
//}
//
//uint64_t block_encrypted::get_length() const
//{
//	return m_length;
//}
//
//const std::vector<uint8_t>& block_encrypted::get_all_data() const
//{
//	return m_buf;
//}
//
//block_encrypted encrypt_from_raw(std::vector<uint8_t>&& data, const uint64_t offset, const Lunaris::RSA_plus& encryption_device)
//{
//	return block_encrypted{ offset, std::move(data), encryption_device };
//}
//
//block_encrypted decrypt_from_raw(std::vector<uint8_t>&& data, const Lunaris::RSA_plus& decryption_device)
//{
//	decryption_device.transform(data);
//	return block_encrypted{ std::move(data), decryption_device };
//}
//
//file_reader_params::file_reader_params(std::vector<uint8_t>&& data, const uint64_t offset, const Lunaris::RSA_plus& encryption_device) noexcept
//	: data(std::move(data)), offset(offset), encrypter(&encryption_device)
//{
//}
//
//file_reader_params::file_reader_params(file_reader_params&& f) noexcept
//	: data(std::move(f.data)), offset(f.offset), encrypter(f.encrypter)
//{
//}
//
//void file_reader_params::operator=(file_reader_params&& f) noexcept
//{
//	data = std::move(f.data);
//	offset = std::exchange(f.offset, 0);
//	encrypter = f.encrypter;
//}
//
//void File_transfer_worker::async_read()
//{
//	//AllegroCPP::File fp(str, "r+");
//	//if (!fp) return;
//
//	auto& fp = m_input; // rename lmao
//
//	//fp.seek(0, ALLEGRO_SEEK_SET); // DO NOT!
//	const auto start = fp.tell();
//
//	int64_t remaining = fp.size() - start;
//	const int64_t total = remaining;
//
//	debug_log("Started reads...");
//
//	switch (this->m_self_type) {
//	case type::SENDER:
//	{
//		while (remaining > 0 && !fp.eof())
//		{
//			const auto current_block_size = file_read_blocks_of < remaining ? file_read_blocks_of : remaining;
//			std::vector<uint8_t> buf(current_block_size, '\0');
//
//			const auto got_read = static_cast<int64_t>(fp.read(buf.data(), buf.size()));
//
//			if (!got_read) continue;
//
//			if (got_read != current_block_size) {
//				buf.resize(got_read);
//			}
//
//			remaining -= got_read;
//
//			m_pool.post(file_reader_params{ std::move(buf), m_offset_counter++, m_encrypter });
//
//			m_progress = 100.0 * (total - remaining) / total;
//
//			if (m_offset_counter > (m_offset_post_counter + file_read_max_pending)) {
//				//debug_log("Overloaded. Read is too fast. Waiting workers...");
//				while (is_overloaded = (m_offset_counter > (m_offset_post_counter + static_cast<uint64_t>(file_read_max_pending * 0.8)))) {
//					std::this_thread::sleep_for(std::chrono::milliseconds(10));
//				}
//				//debug_log("Overload not anymore. Read continues...");
//			}
//		}
//	}
//		break;
//	case type::RECEIVER:
//	{
//
//	}
//		break;
//	}
//
//	
//
//	debug_log("Finished!");
//
//	has_ended_read = true;
//	m_logging_keep_up = false;
//
//	m_logging_keep_up_to_date.stop();
//	m_read_thr.stop();
//}
//
//void File_transfer_worker::async_parallel_criptography(file_reader_params&& frp)
//{
//	// frp.data is the raw read data.
//	// frp.offset is the data offset (as this is async, order may not be the same)
//	// frp.encrypter is used for encryption
//
//	auto prepared_data = encrypt_from_raw(std::move(frp.data), frp.offset, *frp.encrypter);
//	const auto& data_itself = prepared_data.get_all_data();
//
//	{
//		std::lock_guard<std::mutex> l(m_output_mtx);
//		m_output.write(data_itself.data(), data_itself.size());
//	}
//
//	++m_offset_post_counter;
//}
//
//bool File_transfer_worker::async_logging()
//{
//	debug_log(
//		"Async report: R#" + std::to_string(m_offset_counter.load()) + "; "
//		"W#" + std::to_string(m_offset_post_counter.load()) + "; "
//		"LOAD: " + std::to_string(((m_offset_counter - m_offset_post_counter) * 100.0 / file_read_max_pending)) + "%. "
//		"Progress: " + std::to_string(m_progress) + "%"
//	);
//
//	std::this_thread::sleep_for(std::chrono::milliseconds(199));
//	return m_logging_keep_up;
//}
//
//File_transfer_worker::File_transfer_worker(const File_transfer_worker::type typo, AllegroCPP::File& out, AllegroCPP::File& inn, const bool log_it) :
//	Loggable(log_it),
//	m_self_type(typo),
//	//m_encrypter(Lunaris::make_encrypt_auto()),
//	m_pool([this](file_reader_params&& frp) {this->async_parallel_criptography(std::move(frp)); }),
//	m_output(out),
//	m_input(inn)
//{
//	if (log_it) {
//		debug_log("Started logging");
//		m_logging_keep_up = true;
//		m_logging_keep_up_to_date = AllegroCPP::Thread([this] {return this->async_logging(); });
//	}
//
//	switch (m_self_type) {
//	case type::SENDER:
//	{
//		m_encrypter = Lunaris::make_encrypt_auto();
//		// send pre-info data now and start work
//		block_encrypted_info_begin info(m_encrypter.get_combo());
//		const auto& data_itself = info.get_all_data();
//		m_output.write(data_itself.data(), data_itself.size());
//
//		debug_log("Key: " + std::to_string(info.get_key()) + "; Mod: " + std::to_string(info.get_mod()));
//
//		// read all the way
//		m_read_thr = AllegroCPP::Thread([this] { this->async_read(); return false; });
//	}
//		break;
//	case type::RECEIVER:
//	{
//		// Receive enough data of information about encryption
//		std::vector<uint8_t> dat(block_encrypted_info_begin::data_length, '\0');
//		if (inn.read(dat.data(), block_encrypted_info_begin::data_length) != block_encrypted_info_begin::data_length) throw std::runtime_error("Info block missing, can't proceed!");
//
//		block_encrypted_info_begin info(std::move(dat));
//
//		debug_log("Key: " + std::to_string(info.get_key()) + "; Mod: " + std::to_string(info.get_mod()));
//
//		m_encrypter = Lunaris::make_decrypt_auto(Lunaris::RSA_keys<uint64_t>{ info.get_key(), info.get_mod() });
//
//		// read all the way
//		m_read_thr = AllegroCPP::Thread([this] { this->async_read(); return false; });
//	}
//		break;
//	}
//
//}
//
//File_transfer_worker::~File_transfer_worker()
//{
//	while (is_working()) { std::this_thread::yield(); std::this_thread::sleep_for(std::chrono::milliseconds(50)); }
//
//}
//
//bool File_transfer_worker::is_working() const
//{
//	return !has_ended_read || (m_offset_counter != m_offset_post_counter);
//}
//
//uint64_t File_transfer_worker::load_amount() const
//{
//	return m_offset_counter - m_offset_post_counter;
//}
