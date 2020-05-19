#include "filesystem.h"

namespace LSW {
	namespace v5 {
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
		size_t lsw_fread(FILE*& fp, std::string& buf, bool& end, const size_t siz) {
			if (!fp) return 0;
			end = false;

			for (size_t s = 0; s < siz; s++) {
				char ubuf;
				if (!fread_s(&ubuf, 1, sizeof(char), 1, fp)) {
					end = true;
					return s;
				}
				buf += ubuf;
			}
			return siz;
		}
		size_t lsw_fwrite(const std::string& buf, FILE*& fp) {
			if (!fp) return 0;

			for (size_t p = 0; p < buf.length(); p++) {
				if (!fwrite(&buf[p], sizeof(char), 1, fp)) {
					return p;
				}
			}
			return buf.length();
		}
	}
}