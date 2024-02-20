#include "file_reader.h"
#include "displayer.h"

int main()
{
	FrontEnd md;
	bool keep_run = true;

	while (keep_run)
	{
		md.task_draw();
		const auto ev = md.task_events();

		switch (ev) {
		case FrontEnd::e_event::APP_CLOSE:
			keep_run = false;
			break;
		default:
			break;
		}

	}

//#ifdef _DEBUG
//	file_reader_test();
//#endif

	return 0;
}



//#include <AllegroCPP/include/System.h>
//#include <AllegroCPP/include/Graphics.h>
//#include <AllegroCPP/include/Audio.h>
//#include <Lunaris/Encryption/encryption.h>


//#define SMALL "small_"
//#define SMALL ""

//const char origin_file[] = "video.mov";
//const char encryp_file[] = "encrypted_video.enc";
//const char retrie_file[] = "restored_video.mov";

//AllegroCPP::Text_log logg("Logger");
//
//logg << "Starting in 3 sec" << std::endl;
//
//std::this_thread::sleep_for(std::chrono::seconds(3));
//
//{
//	AllegroCPP::File read_from(origin_file, "rb");
//	AllegroCPP::File write_to(encryp_file, "wb");
//
//	//File_transfer_worker fre(File_transfer_worker::type::SENDER, write_to, read_from, true);
//	auto wrk = make_encrypter(write_to, read_from);
//	wrk.link_logger(logg);
//	wrk.start_async();
//
//	logg << "Look, this is async!" << std::endl;
//}
//
//std::this_thread::sleep_for(std::chrono::seconds(1));
//
//{
//	AllegroCPP::File read_from(encryp_file, "rb");
//	AllegroCPP::File write_to(retrie_file, "wb");
//
//	//File_transfer_worker fre(File_transfer_worker::type::RECEIVER, write_to, read_from, true);
//	auto wrk = make_decrypter(write_to, read_from);
//	wrk.link_logger(logg);
//	wrk.start_async();
//
//	logg << "Look, this is async!" << std::endl;
//}