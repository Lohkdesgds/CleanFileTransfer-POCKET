#include "file_reference.h"
#include "app.h"

#define NOWW std::chrono::system_clock::now()

int main()
{
	std::atomic_int step_counter = 0;

	App* app = nullptr;
	AllegroCPP::Thread thr_draw([&] {
		if (!app) {
			app = new App();
			++step_counter;
		}
		return app->draw();
	});

	while (step_counter == 0) std::this_thread::sleep_for(std::chrono::milliseconds(10));

	AllegroCPP::Thread thr_async([&app] {
		return app->think();
	});

	while (app->running()) {
		std::this_thread::sleep_for(std::chrono::milliseconds(4000));
		const auto ptr = app->get_next_file_to_transfer();
		if (ptr) {
			ptr->set_status(File_reference::e_status::SENDING);
			for (double d = 0.0; d <= 1.0; d += 0.01) {
				ptr->set_progress(d);
				std::this_thread::sleep_for(std::chrono::milliseconds(50));
			}
			ptr->set_progress(1.0);
			ptr->set_status(File_reference::e_status::ENDED_TRANSFER);

			app->move_to_bottom_this_file(ptr);
		}
	}

	delete app;

	return 0;
}

//FrontEnd md;
//bool keep_run = true;
//
//AllegroCPP::Thread async_ev([&] {
//	const auto ev = md.task_events();
//
//	switch (ev) {
//	case FrontEnd::e_event::APP_CLOSE:
//		keep_run = false;
//		return false;
//	case FrontEnd::e_event::WANT_DISCONNECT:
//		printf_s("Would disconnect\n");
//		md.set_connection_status(FrontEnd::e_connection_status::DISCONNECTED);
//		break;
//	case FrontEnd::e_event::WANT_CONNECT:
//		printf_s("Would connect as %s\n", md.get_connection_mode_selected() == FrontEnd::e_connection_mode::CLIENT ? "CLIENT" : "HOST");
//		md.set_connection_status(FrontEnd::e_connection_status::CONNECTED);
//		break;
//	case FrontEnd::e_event::POSTED_NEW_ITEMS:
//	{
//		for (auto it = md.get_next_ready_to_send_ref(); it; it = md.get_next_ready_to_send_ref())
//			it->set_status(static_cast<File_reference::e_status>(rand() % 5));
//	}
//		break;
//	default:
//		break;
//	}
//	return true;
//});
//
//while (keep_run) md.task_draw();

//#ifdef _DEBUG
//	file_reader_test();
//#endif


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