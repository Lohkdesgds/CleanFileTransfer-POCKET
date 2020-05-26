#include "filesystem.h"

namespace LSW {
	namespace v5 {
		std::string SmartFILE::convert(const file_modes m)
		{
			switch (m) {
			case file_modes::READ:
				return "rb";
			case file_modes::WRITE:
				return "wb";
			case file_modes::READ_WRITE_KEEP:
				return "rb+";
			case file_modes::READ_WRITE_OVERWRITE:
				return "wb+";
			case file_modes::APPEND_WRITE:
				return "ab";
			case file_modes::APPEND_READ_WRITE:
				return "ab+";
			}
			return "";
		}
		SmartFILE::~SmartFILE()
		{
			close();
		}
		bool SmartFILE::open(string_sized path, const file_modes m)
		{
			eoff = false;
			close();
			Tools::interpretPath(path);
			size_of_this = Tools::getFileSize(path);
			return (fopen_s(&fp, path.c_str(), convert(m).c_str()) == 0);
		}

		long long SmartFILE::totalSize()
		{
			return size_of_this;
		}

		void SmartFILE::close()
		{
			if (fp) {
				fclose(fp);
				eoff = true;
				fp = nullptr;
			}
		}

		bool SmartFILE::eof()
		{
			if (fp) return eoff;
			return true;
		}

		size_t SmartFILE::read(string_sized& buf, const size_t siz)
		{
			if (!fp) { eoff = true; return 0; }
			if (feof(fp)) { eoff = true; return 0;}

			char* temp = new char[siz];

			auto readd = fread_s(temp, siz, sizeof(char), siz, fp);
			if (readd <= 0) {
				eoff = true;
				return 0;
			}
			for (size_t u = 0; u < readd; u++) buf += temp[u];
			delete[] temp;
			return readd;
		}

		size_t SmartFILE::write(string_sized& buf, const size_t s)
		{
			if (!fp) return 0;
			return fwrite(buf.data(), sizeof(char), s ? s : buf.size(), fp);
		}



		/*
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

			buf.reserve(siz);
			auto read = fread_s(buf.data(), siz, sizeof(char), siz, fp);
			end = (read != siz) || feof(fp);
			buf._set_size(read);
			return read;
		}
		size_t lsw_fwrite(FILE*& fp, const string_sized& buf) {
			if (!fp) return 0;
			return fwrite(buf.data(), sizeof(char), buf.size(), fp);
		}
		*/
	}
}