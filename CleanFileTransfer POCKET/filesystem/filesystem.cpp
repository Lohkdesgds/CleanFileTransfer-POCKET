#include "filesystem.h"

namespace LSW {
	namespace v5 {
		/*void string_sized::realloc(const size_t s)
		{
			if (buf_s < s) {
				if (buf) delete[] buf;
				buf = new char[s + 1];
				buf[s] = '\0';
				buf_s = s;
			}
		}
		string_sized::string_sized(const std::string& s)
		{
			realloc(s.size());
			memcpy(buf, s.c_str(), s.size());
			len = s.size();
		}
		string_sized::string_sized(const char str[])
		{
			realloc(sizeof(str));
			for (size_t u = 0; u < sizeof(str); u++) buf[u] = str[u];
			len = sizeof(str);
		}
		string_sized::~string_sized()
		{
			clear();
		}
		void string_sized::set(const char* s, const size_t siz)
		{
			realloc(siz);
			len = siz;
			memcpy_s(buf, siz, s, siz);
		}
		char* string_sized::data(const size_t minbuf)
		{
			realloc(minbuf);
			return buf;
		}
		const char* string_sized::data() const
		{
			return buf;
		}
		const char* string_sized::c_str() const
		{
			return buf;
		}
		void string_sized::_setSize(const size_t news)
		{
			len = news;
		}*/
		/*
		std::string& string_sized::s_str()
		{
			return buf;
		}
		const std::string& string_sized::s_str() const
		{
			return buf;
		}*/
		/*const size_t& string_sized::size() const
		{
			return len;
		}

		const size_t& string_sized::capacity() const
		{
			return buf_s;
		}

		void string_sized::clear()
		{
			if (buf) {
				delete[] buf;
				buf = nullptr;
				buf_s = 0;
				len = 0;
			}
		}*/

		/*void string_sized::erase(const size_t s)
		{
			buf.erase(buf.begin() + s);
			real_size--;
		}*/

		/*void string_sized::erase(const size_t s, const size_t clen)
		{
			if (clen + s >= buf_s) throw Abort::Abort(__FUNCSIG__, "Cannot erase out of range!", Abort::abort_level::FATAL_ERROR, [&]() {clear(); });
			size_t data_end = len - clen;
			char* n_buf = new char[data_end];
			size_t p_o = 0, p_n = 0;

			for (size_t p = 0; p < s; p++) {
				n_buf[p_n++] = buf[p_o++];
			}
			p_o += clen;
			while (p_n < data_end) {
				n_buf[p_n++] = buf[p_o++];
			}
			char* _t = buf;
			buf = n_buf;
			delete[] buf;
			len = data_end;
		}

		const char& string_sized::operator[](const size_t p) const
		{
			return buf[p];
		}

		const bool string_sized::operator==(const string_sized s)
		{
			//return buf == s.s_str();
			return memcmp(s.data(), data(), s.size() < size() ? s.size() : size()) == 0;
		}*/


		errno_t lsw_fopen(FILE*& fp, const char* path, const char* mode)
		{
			std::string npath = path;
			Tools::interpretPath(npath);
			return fopen_s(&fp, npath.c_str(), mode);
		}
		errno_t lsw_fopen_f(FILE*& fp, const char* path, const char* mode)
		{
			std::string npath = path;
			Tools::handlePath(npath);
			return fopen_s(&fp, npath.c_str(), mode);
		}
		size_t lsw_fread(FILE*& fp, string_sized& buf, bool& end, size_t siz) {
			if (!fp) return 0;
			end = false;

			/*char cutestaminabuf[1 << 6];
			buf.reserve(siz); // maybe it does.
			
			for (size_t s = 0; s < siz;) {
				size_t read = fread_s(cutestaminabuf, 1 << 6, sizeof(char), 1 << 6, fp);
				if (read <= 0) {
					end = true;
					return s;
				}
				s += read;
				for (size_t u = 0; u < read; u++) {
					buf += cutestaminabuf[u];
				}
			}
			return siz;*/


			/*for (size_t s = 0; s < siz; s++) {
				char ubuf;
				if (!fread_s(&ubuf, 1, sizeof(char), 1, fp)) {
					end = true;
					return s;
				}
				buf += ubuf;
			}
			return siz;*/

			buf.reserve(siz);
			auto read = fread_s(buf.data(), siz, sizeof(char), siz, fp);
			end = (read != siz) || feof(fp);
			buf._set_size(read);
			return read;
		}
		size_t lsw_fwrite(FILE*& fp, const string_sized& buf) {
			if (!fp) return 0;

			/*for (size_t p = 0; p < buf.size(); p++) {
				if (!fwrite(&buf[p], sizeof(char), 1, fp)) {
					return p;
				}
			}
			return buf.length();*/
			return fwrite(buf.data(), sizeof(char), buf.size(), fp);
		}
	}
}