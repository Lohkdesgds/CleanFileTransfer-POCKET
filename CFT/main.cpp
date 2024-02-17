//#include <AllegroCPP/include/System.h>
//#include <AllegroCPP/include/Graphics.h>
//#include <AllegroCPP/include/Audio.h>
//#include <Lunaris/Encryption/encryption.h>

#include "file_reader.h"

int main()
{
	//AllegroCPP::File testfile("file.txt");
	//
	//auto enc = Lunaris::make_encrypt_auto();
	//
	//std::vector<uint8_t> test = { 'a', 'b', 'c' };
	//
	//auto blk = encrypt_from_raw(std::move(test), 0, enc);
	//
	//auto keys = enc.get_combo();

	AllegroCPP::File read_from("file_test.txt", "rb");
	AllegroCPP::File write_to("file_written.txt", "wb");


	File_reader_encrypter fre(write_to, read_from, true);

	while (fre.is_working()) std::this_thread::sleep_for(std::chrono::seconds(5));

	std::this_thread::sleep_for(std::chrono::seconds(5));
	return 0;
}