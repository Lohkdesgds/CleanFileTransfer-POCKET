//#include <AllegroCPP/include/System.h>
//#include <AllegroCPP/include/Graphics.h>
//#include <AllegroCPP/include/Audio.h>
//#include <Lunaris/Encryption/encryption.h>

#include "file_reader.h"

//#define SMALL "small_"
//#define SMALL ""

const char origin_file[] = "video.mov";
const char encryp_file[] = "encrypted_video.enc";
const char retrie_file[] = "restored_video.mov";

int main()
{
	AllegroCPP::Text_log logg("Logger");

	logg << "Starting in 3 sec" << std::endl;

	std::this_thread::sleep_for(std::chrono::seconds(3));

	{
		AllegroCPP::File read_from(origin_file, "rb");
		AllegroCPP::File write_to(encryp_file, "wb");

		//File_transfer_worker fre(File_transfer_worker::type::SENDER, write_to, read_from, true);
		auto wrk = make_sender(write_to, read_from);
		wrk.link_logger(logg);
		wrk.start_async();

		logg << "Look, this is async!" << std::endl;
	}

	std::this_thread::sleep_for(std::chrono::seconds(1));

	{
		AllegroCPP::File read_from(encryp_file, "rb");
		AllegroCPP::File write_to(retrie_file, "wb");

		//File_transfer_worker fre(File_transfer_worker::type::RECEIVER, write_to, read_from, true);
		auto wrk = make_receiver(write_to, read_from);
		wrk.link_logger(logg);
		wrk.start_async();

		logg << "Look, this is async!" << std::endl;
	}


	return 0;
}