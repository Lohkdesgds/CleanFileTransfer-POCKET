#include "file_reader.h"


block_encrypted_info_begin::block_encrypted_info_begin(const Lunaris::RSA_keys<uint64_t>& k)
{
	m_buf.resize(sizeof(uint64_t) * 2);
	uint64_t* _key = (uint64_t*)m_buf.data();
	uint64_t* _mod = (uint64_t*)m_buf.data() + 1;
	*_key = k.key;
	*_mod = k.mod;
}

block_encrypted_info_begin::block_encrypted_info_begin(std::vector<uint8_t>&& data)
	: m_buf(std::move(data))
{
}

uint64_t block_encrypted_info_begin::get_key() const
{
	uint64_t* _key = (uint64_t*)m_buf.data();
	return *_key;
}

uint64_t block_encrypted_info_begin::get_mod() const
{
	uint64_t* _mod = (uint64_t*)m_buf.data() + 1;
	return *_mod;
}

const std::vector<uint8_t>& block_encrypted_info_begin::get_all_data() const
{
	return m_buf;
}

block_encrypted::block_encrypted(const uint64_t order, std::vector<uint8_t>&& data, const Lunaris::RSA_plus& encryption_device)
{
	m_buf.resize(sizeof(uint64_t) * 2);

	uint64_t* _order = (uint64_t*)m_buf.data();
	uint64_t* _length = (uint64_t*)m_buf.data() + 1;

	m_order = *_order = order;
	m_length = *_length = static_cast<uint64_t>(data.size());

	m_buf.insert(m_buf.end(), data.begin(), data.end());
	data.clear();

	encryption_device.transform(m_buf);
}

block_encrypted::block_encrypted(std::vector<uint8_t>&& data, const Lunaris::RSA_plus& decryption_device)
{
	decryption_device.transform(data);

	uint64_t* _order = (uint64_t*)data.data();
	uint64_t* _length = (uint64_t*)data.data() + 1;

	m_order = *_order;
	m_length = *_length;

	data.erase(data.begin(), data.begin() + (sizeof(uint64_t) / sizeof(uint8_t)) * 2);

	m_buf = std::move(data);
}

uint64_t block_encrypted::get_order() const
{
	return m_order;
}

uint64_t block_encrypted::get_length() const
{
	return m_length;
}

const std::vector<uint8_t>& block_encrypted::get_all_data() const
{
	return m_buf;
}

block_encrypted encrypt_from_raw(std::vector<uint8_t>&& data, const uint64_t offset, const Lunaris::RSA_plus& encryption_device)
{
	return block_encrypted{ offset, std::move(data), encryption_device };
}

block_encrypted decrypt_from_raw(std::vector<uint8_t>&& data, const Lunaris::RSA_plus& decryption_device)
{
	decryption_device.transform(data);
	return block_encrypted{ std::move(data), decryption_device };
}

file_reader_params::file_reader_params(std::vector<uint8_t>&& data, const uint64_t offset, const Lunaris::RSA_plus& encryption_device) noexcept
	: data(std::move(data)), offset(offset), encrypter(&encryption_device)
{
}

file_reader_params::file_reader_params(file_reader_params&& f) noexcept
	: data(std::move(f.data)), offset(f.offset), encrypter(f.encrypter)
{
}

void file_reader_params::operator=(file_reader_params&& f) noexcept
{
	data = std::move(f.data);
	offset = std::exchange(f.offset, 0);
	encrypter = f.encrypter;
}

void File_reader_encrypter::debug_log(const std::string& s)
{
	if (m_logging) *m_logging << s << std::endl;
}

void File_reader_encrypter::async_read()
{
	//AllegroCPP::File fp(str, "r+");
	//if (!fp) return;

	auto& fp = m_input; // rename lmao

	fp.seek(0, ALLEGRO_SEEK_SET);

	int64_t remaining = fp.size();
	const int64_t total = remaining;

	debug_log("Started reads...");

	while(remaining > 0 && !fp.eof())
	{
		const auto current_block_size = file_read_blocks_of < remaining ? file_read_blocks_of : remaining;
		std::vector<uint8_t> buf(current_block_size, '\0');

		const auto got_read = static_cast<int64_t>(fp.read(buf.data(), buf.size()));

		if (!got_read) continue;

		if (got_read != current_block_size) {
			buf.resize(got_read);
		}

		remaining -= got_read;

		m_pool.post(file_reader_params{ std::move(buf), m_offset_counter++, m_encrypter });

		m_progress = 100.0 * (total - remaining) / total;

		if (m_offset_counter > (m_offset_post_counter + file_read_max_pending)) {
			//debug_log("Overloaded. Read is too fast. Waiting workers...");
			while (is_overloaded = (m_offset_counter > (m_offset_post_counter + static_cast<uint64_t>(file_read_max_pending * 0.8)))) {
				std::this_thread::sleep_for(std::chrono::milliseconds(10));
			}
			//debug_log("Overload not anymore. Read continues...");
		}
	}

	debug_log("Finished!");

	has_ended_read = true;
	m_logging_keep_up = false;

	m_logging_keep_up_to_date.stop();
	m_read_thr.stop();
}

void File_reader_encrypter::async_parallel_encrypt(file_reader_params&& frp)
{
	// frp.data is the raw read data.
	// frp.offset is the data offset (as this is async, order may not be the same)
	// frp.encrypter is used for encryption

	auto prepared_data = encrypt_from_raw(std::move(frp.data), frp.offset, *frp.encrypter);
	const auto& data_itself = prepared_data.get_all_data();

	{
		std::lock_guard<std::mutex> l(m_output_mtx);
		m_output.write(data_itself.data(), data_itself.size());
	}

	++m_offset_post_counter;
}

bool File_reader_encrypter::async_logging()
{
	*m_logging << "Async report: R#" << m_offset_counter.load() << "; W#" << m_offset_post_counter.load() << "; LOAD: " << (((m_offset_counter - m_offset_post_counter) * 100.0 / file_read_max_pending)) << "%. Progress: " << m_progress << "%" << std::endl;
	std::this_thread::sleep_for(std::chrono::milliseconds(199));
	return m_logging_keep_up;
}

File_reader_encrypter::File_reader_encrypter(AllegroCPP::File& out, AllegroCPP::File& inn, const bool log_it)
	: m_pool([this](file_reader_params&& frp) {this->async_parallel_encrypt(std::move(frp)); }), m_output(out), m_input(inn)
{
	if (log_it) {
		m_logging = new AllegroCPP::Text_log("File reader encrypter");
		*m_logging << "Started logging" << std::endl;

		m_logging_keep_up = true;
		m_logging_keep_up_to_date = AllegroCPP::Thread([this] {return this->async_logging(); });
	}

	// send pre-info data now and start work
	block_encrypted_info_begin info(m_encrypter.get_combo());
	const auto& data_itself = info.get_all_data();
	m_output.write(data_itself.data(), data_itself.size());

	// read all the way
	m_read_thr = AllegroCPP::Thread([this] { this->async_read(); return false; });
}

File_reader_encrypter::~File_reader_encrypter()
{
	m_logging_keep_up = false;
	if (m_logging) delete m_logging;
	m_logging = nullptr;
}

bool File_reader_encrypter::is_working() const
{
	return !has_ended_read || (m_offset_counter != m_offset_post_counter);
}

uint64_t File_reader_encrypter::load_amount() const
{
	return m_offset_counter - m_offset_post_counter;
}
