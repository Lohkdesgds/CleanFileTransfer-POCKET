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

		enum class file_modes {READ = 1, WRITE, READ_WRITE_KEEP, READ_WRITE_OVERWRITE, APPEND_WRITE, APPEND_READ_WRITE};

		class SmartFILE {
			FILE* fp = nullptr;
			int mode = 0;
			long long size_of_this = 0;
			bool eoff = false;

			std::string convert(const file_modes);
		public:
			~SmartFILE();

			bool open(std::string, const file_modes);
			long long totalSize();
			void close();
			bool eof();

			size_t read(std::string&, const size_t);
			size_t write(std::string&, const size_t = 0);
		};
	}
}