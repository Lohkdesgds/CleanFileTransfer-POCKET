#pragma once

// C
#include <stdio.h>
#include <stdlib.h>
// C++
#include <string>

// import
#include "..\tools\tools.h"


namespace LSW {
	namespace v5 {

		class string_sized : public std::string {
			void copy_mem(const char* buf, size_t len) {
				/*char* end = nullptr;
				if (size() > 0) end = &back();
				resize(size() + len);
				if (!end) end = data();
				else end++;
				memcpy_s(end, len, buf, len);*/
				for (size_t u = 0; u < len; u++) *this += buf[u];
			}
		public:
			string_sized() = default;
			template<typename... Args> string_sized(Args... args) : std::string(args...) { }

			// FULL CONST

			string_sized(const string_sized& oth) : std::string() {
				copy_mem(oth.data(), oth.length() ? oth.length() : oth.size());
			}
			template<size_t _size> string_sized(const char(oth)[_size]) : std::string() {
				copy_mem(oth, _size);
			}
			string_sized(const char* oth) : std::string() {
				copy_mem(oth, strlen(oth));
			}
			string_sized(const char* oth, const size_t len) : std::string() {
				copy_mem(oth, len);
			}

			// NON CONST

			string_sized(string_sized& oth) : std::string() {
				copy_mem(oth.data(), oth.length() ? oth.length() : oth.size());
			}
			template<size_t _size> string_sized(char(oth)[_size]) : std::string() {
				copy_mem(oth, _size);
			}
			string_sized(char* oth) : std::string() {
				copy_mem(oth, strlen(oth));
			}
			string_sized(char* oth, const size_t len) : std::string() {
				copy_mem(oth, len);
			}

			// FULL CONST

			string_sized& operator=(const string_sized& oth) noexcept {
				clear();
				copy_mem(oth.data(), oth.length() ? oth.length() : oth.size());
				return *this;
			}
			string_sized& operator+=(const string_sized& oth) noexcept {
				copy_mem(oth.data(), oth.length() ? oth.length() : oth.size());
				return *this;
			}
			template<size_t _size> string_sized& operator=(const char (oth)[_size]) noexcept {
				clear();
				copy_mem(oth, _size);
				return *this;
			}
			template<size_t _size> string_sized& operator+=(const char(oth)[_size]) noexcept {
				copy_mem(oth, _size);
				return *this;
			}
			string_sized& operator=(const char* oth) noexcept {
				clear();
				copy_mem(oth, strlen(oth));
				return *this;
			}
			string_sized& operator+=(const char* oth) noexcept {
				copy_mem(oth, strlen(oth));
				return *this;
			}
			string_sized& operator=(const char& oth) noexcept {
				clear();
				copy_mem(&oth, 1);
				return *this;
			}
			string_sized& operator+=(const char& oth) noexcept {
				std::string::operator+=(oth);
				return *this;
			}

			// NON CONST

			string_sized& operator=(string_sized& oth) noexcept {
				clear();
				copy_mem(oth.data(), oth.length() ? oth.length() : oth.size());
				return *this;
			}
			string_sized& operator+=(string_sized& oth) noexcept {
				copy_mem(oth.data(), oth.length() ? oth.length() : oth.size());
				return *this;
			}
			template<size_t _size> string_sized& operator=(char (oth)[_size]) noexcept {
				clear();
				copy_mem(oth, _size);
				return *this;
			}
			template<size_t _size> string_sized& operator+=(char(oth)[_size]) noexcept {
				copy_mem(oth, _size);
				return *this;
			}
			string_sized& operator=(char* oth) noexcept {
				clear();
				copy_mem(oth, strlen(oth));
				return *this;
			}
			string_sized& operator+=(char* oth) noexcept {
				copy_mem(oth, strlen(oth));
				return *this;
			}
			string_sized& operator=(char& oth) noexcept {
				clear();
				copy_mem(&oth, 1);
				return *this;
			}
			string_sized& operator+=(char& oth) noexcept {
				std::string::operator+=(oth);
				return *this;
			}

			// STD

			template<typename T> string_sized& operator+=(T& item) {
				std::string::operator+=(item); return *this;
			}
			template<typename T> string_sized& operator=(T& item) {
				std::string::operator=(item); return *this;
			}

			// FULL CONST

			string_sized operator+(const string_sized& oth) const {
				string_sized combo;
				combo += *this;
				combo += oth;
				return combo;
			}
			template<size_t _size> string_sized operator+(const char (oth)[_size]) const {
				string_sized combo;
				combo += *this;
				combo += oth;
				return combo;
			}
			string_sized operator+(const char* oth) const {
				string_sized combo;
				combo += *this;
				combo += oth;
				return combo;
			}
			string_sized operator+(const char& oth) const {
				string_sized combo;
				combo += *this;
				combo += oth;
				return combo;
			}

			// NON CONST

			string_sized operator+(string_sized& oth) const {
				string_sized combo;
				combo += *this;
				combo += oth;
				return combo;
			}
			template<size_t _size> string_sized operator+(char(oth)[_size]) const {
				string_sized combo;
				combo += *this;
				combo += oth;
				return combo;
			}
			string_sized operator+(char* oth) const {
				string_sized combo;
				combo += *this;
				combo += oth;
				return combo;
			}
			string_sized operator+(char& oth) const {
				string_sized combo;
				combo += *this;
				combo += oth;
				return combo;
			}

			// STD

			template<typename T> string_sized& operator+(T& item) {
				std::string::operator+(item); return *this;
			}

			// FULL CONST

			inline int compare(const string_sized& oth) noexcept { return memcmp(data(), oth.data(), size() < oth.size() ? size() : oth.size()); }
			inline int compare(const std::string& oth) noexcept { return memcmp(data(), oth.data(), size() < oth.size() ? size() : oth.size()); }
			template<size_t _size> int compare(const char (&oth)[_size]) noexcept { return memcmp(data(), oth, size() < _size ? size() : _size); }
			inline int compare(const char* &oth) noexcept { auto _len = strlen(oth); return memcmp(data(), oth, size() < _len ? size() : _len); }

			// NON CONST

			inline int compare(string_sized& oth) noexcept { return memcmp(data(), oth.data(), size() < oth.size() ? size() : oth.size()); }
			inline int compare(std::string& oth) noexcept { return memcmp(data(), oth.data(), size() < oth.size() ? size() : oth.size()); }
			template<size_t _size> int compare(char(&oth)[_size]) noexcept { return memcmp(data(), oth, size() < _size ? size() : _size); }
			inline int compare(char*& oth) noexcept { auto _len = strlen(oth); return memcmp(data(), oth, size() < _len ? size() : _len); }

			// FULL CONST

			inline bool operator==(const string_sized& oth) noexcept { return this->compare(oth) == 0; }
			inline bool operator==(const std::string& oth) noexcept { return this->compare(oth) == 0; }
			template<size_t _size> bool operator==(const char (&oth)[_size]) noexcept { return this->compare(oth) == 0; }
			inline bool operator==(const char* &oth) noexcept { return this->compare(oth) == 0; }

			// NON CONST

			inline bool operator==(string_sized& oth) noexcept { return this->compare(oth) == 0; }
			inline bool operator==(std::string& oth) noexcept { return this->compare(oth) == 0; }
			template<size_t _size> bool operator==(char (&oth)[_size]) noexcept { return this->compare(oth) == 0; }
			inline bool operator==(char* &oth) noexcept { return this->compare(oth) == 0; }

			// FULL CONST

			inline void cat(const char* s, const size_t t) {
				copy_mem(s, t);
			}
			/*inline void cat(const char (s)[], const size_t t) {
				copy_mem(s, t);
			}*/

			// NON CONST

			/*inline void cat(char* s, const size_t t) {
				copy_mem(s, t);
			}
			inline void cat(char (s)[], const size_t t) {
				copy_mem(s, t);
			}*/

			string_sized substr(const size_t off, const size_t len = static_cast<size_t>(-1)) {
				return string_sized(data() + off, (len >= size() - off) ? size() - off : len);
			}
		};

		/*template<typename... Args> string_sized to_string_sized(Args... args)
		{
			std::string str = std::to_string(args...);
			return { str.c_str(), str.length() };
		}*/


		enum class file_modes {READ = 1, WRITE, READ_WRITE_KEEP, READ_WRITE_OVERWRITE, APPEND_WRITE, APPEND_READ_WRITE};

		class SmartFILE {
			FILE* fp = nullptr;
			int mode = 0;
			long long size_of_this = 0;
			bool eoff = false;

			std::string convert(const file_modes);
		public:
			~SmartFILE();

			bool open(string_sized, const file_modes);
			long long totalSize();
			void close();
			bool eof();

			size_t read(string_sized&, const size_t);
			size_t write(string_sized&, const size_t = 0);
		};


		/*
		errno_t lsw_fopen(FILE*&, const char*, const char*);
		errno_t lsw_fopen_f(FILE*&, const char*, const char*);

		// "slower" but reliable fread
		size_t lsw_fread(FILE*&, string_sized&, bool&, size_t = static_cast<size_t>(-1));
		// "slower" but reliable fwrite
		size_t lsw_fwrite(FILE*&, const string_sized&);*/
	}
}