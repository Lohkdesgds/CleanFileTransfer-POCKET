#pragma once

#include <LSWv5.h>

using namespace LSW::v5;

namespace Custom {


	class TransferHelp {
		Tools::SuperMutex mtx;
		std::vector<std::pair<uintptr_t, uintptr_t>> linkage; // from, to (who send, who recv)
		std::vector<uintptr_t> busy_send;
	public:
		// RECEIVE, SENDER
		bool add_me_on_sender(const uintptr_t, const uintptr_t);

		// SENDER START SENDING!
		void set_transfering(const uintptr_t);
		// SENDER ENDED TRANSFER, REMOVE ANY LINKED TO IT
		//void remove_transfering(const uintptr_t);
		// LIST USERS WAITING FOR (MY) DATA
		std::vector<uintptr_t> list_transf(const uintptr_t);

		// removes user from any key
		void cleanup_user(const uintptr_t);
	};
}