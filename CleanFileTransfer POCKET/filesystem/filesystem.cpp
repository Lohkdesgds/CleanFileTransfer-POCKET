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
		bool SmartFILE::open(std::string path, const file_modes m)
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
			return eoff;
		}

		size_t SmartFILE::read(std::string& buf, const size_t siz)
		{
			if (!fp) { return 0; }
			if (feof(fp)) { eoff = true; return 0;}

			buf.resize(siz); // set maximum size

			auto readd = fread_s(buf.data(), siz, sizeof(char), siz, fp);
			if (readd <= 0) {
				eoff = true;
				buf.clear();
				return 0;
			}
			buf.resize(readd); // set what is there
			buf.shrink_to_fit(); // clean up the rest of it (won't clear after it)
			return readd;
		}

		size_t SmartFILE::write(std::string& buf, const size_t s)
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