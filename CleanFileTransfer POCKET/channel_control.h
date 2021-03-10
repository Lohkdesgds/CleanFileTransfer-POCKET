#pragma once

#include <LSWv5.h>

using namespace LSW::v5;

namespace Custom {

	// please don't set below 2
	const std::chrono::seconds time_typing_limit = std::chrono::seconds(5);

	struct user_info {
		uintptr_t id_i;
		std::weak_ptr<Interface::Connection> id_ptr;
		std::string channel;
		std::string ident;
		std::chrono::seconds last_typing;
	};

	class Channels {
		Tools::SuperMutex mtx;
		std::vector<user_info> chs;
	public:
		void add(std::weak_ptr<Interface::Connection>);
		void set(const uintptr_t, const std::string&);
		void set_nick(const uintptr_t, const std::string&);
		std::string channel_of(const uintptr_t);
		std::string nick_of(const uintptr_t);
		void set_typing(const uintptr_t);
		void remove(const uintptr_t);
		std::shared_ptr<Interface::Connection> get_user(const uintptr_t);
		size_t amount_of_users_typing(const std::string&);

		std::vector<user_info> list(const std::string&);
	};
}