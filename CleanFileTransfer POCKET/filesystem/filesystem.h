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

		errno_t lsw_fopen(FILE*&, const char*, const char*);
		errno_t lsw_fopen_f(FILE*&, const char*, const char*);

		// "slower" but reliable fread
		size_t lsw_fread(FILE*&, std::string&, bool&, const size_t = static_cast<size_t>(-1));
		// "slower" but reliable fwrite
		size_t lsw_fwrite(const std::string&, FILE*&);
	}
}