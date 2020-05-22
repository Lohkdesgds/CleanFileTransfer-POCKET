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

		/*class string_sized {
			///std::string buf;
			char* buf = nullptr;
			size_t buf_s = 0;
			size_t len = 0;

			void realloc(const size_t);
		public:
			string_sized() = default;
			string_sized(const std::string&);
			string_sized(const char[]);
			~string_sized();

			// set as
			void set(const char*, const size_t);

			// guarantee at leas size_t buf (reserve)
			char* data(const size_t);
			// guarantee at leas size_t buf (reserve)
			const char* data() const;

			// guarantee at leas size_t buf (reserve)
			const char* c_str() const;

			// return the string itself
			///std::string& s_str();
			// return the string itself
			///const std::string& s_str() const;

			// forces size of it
			void _setSize(const size_t);

			// size of the content inside
			const size_t& size() const;
			// data allocated (not necessarily the size of the content inside)
			const size_t& capacity() const;

			void clear();
			//void erase(const size_t);
			void erase(const size_t, const size_t = static_cast<size_t>(-1));

			const char& operator[](const size_t) const;

			const bool operator==(const string_sized);
		};*/

		class string_sized : public std::string {
			size_t new_size, last_reserved = 0;
			void __clrmss() { new_size = last_reserved = 0; }
		public:
			template<typename... Args> string_sized(Args... args) : std::string(args...) { new_size = 0; }
			template<> string_sized(string_sized& oth) : std::string() { for (size_t k = 0; k < oth.size(); k++) *this += oth[k]; new_size = oth.size(); }

			void _set_size(const size_t s) noexcept { new_size = s; }
			size_t size() const noexcept { return new_size ? new_size : (std::string::size() != 0 ? std::string::size() : last_reserved); };
			size_t length() const noexcept { return new_size ? new_size : (std::string::size() != 0 ? std::string::size() : last_reserved); };
			void reserve(const size_t ns) noexcept { std::string::reserve(ns); last_reserved = ns; }

			void clear() noexcept { std::string::clear(); __clrmss(); }
			bool empty() noexcept { return new_size ? false : std::string::empty(); }
			template<typename... Args> string_sized& operator+=(Args... args) noexcept { __clrmss(); std::string::operator+=(args...); return *this; }
			template<typename... Args> string_sized& operator=(Args... args) noexcept { __clrmss(); std::string::operator=(args...); return *this; }
			template<> string_sized& operator=(string_sized& oth) noexcept { __clrmss(); clear(); for (size_t k = 0; k < oth.size(); k++) *this += oth[k]; new_size = oth.size(); return *this; }
			template<typename... Args> string_sized& append(Args... args) noexcept { __clrmss(); std::string::append(args...); return *this; }
			template<typename... Args> void push_back(Args... args) noexcept { __clrmss(); std::string::push_back(args...); }
			template<typename... Args> string_sized& assign(Args... args) noexcept { __clrmss(); std::string::assign(args...); return *this; }
			template<typename... Args> auto insert(Args... args) noexcept { __clrmss(); return std::string::insert(args...); }
			template<typename... Args> auto erase(Args... args) noexcept { __clrmss(); return std::string::erase(args...); }
			template<typename... Args> string_sized& replace(Args... args) noexcept { __clrmss(); std::string::replace(args...); return *this; }
			void swap(string_sized& oth) noexcept { new_size = oth.size(); std::string::swap(oth); }
			bool pop_back() noexcept { __clrmss(); if (size() > 0) { std::string::pop_back(); return true; } return false; }
			int compare(const string_sized& oth) noexcept { return memcmp(data(), oth.data(), size() < oth.size() ? size() : oth.size()); }
			bool operator==(const string_sized& oth) noexcept { return this->compare(oth) == 0; }
			bool operator==(const char oth[]) noexcept { return this->compare(oth) == 0; }
			bool operator==(const std::string& oth) noexcept { return this->compare(oth) == 0; }
		};

		errno_t lsw_fopen(FILE*&, const char*, const char*);
		errno_t lsw_fopen_f(FILE*&, const char*, const char*);

		// "slower" but reliable fread
		size_t lsw_fread(FILE*&, string_sized&, bool&, size_t = static_cast<size_t>(-1));
		// "slower" but reliable fwrite
		size_t lsw_fwrite(FILE*&, const string_sized&);
	}
}