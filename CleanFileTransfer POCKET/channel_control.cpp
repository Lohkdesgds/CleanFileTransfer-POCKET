#include "channel_control.h"

namespace Custom {

	void Channels::add(std::weak_ptr<Interface::Connection> ptr)
	{
		Tools::AutoLock l(mtx);
		chs.push_back({ (uintptr_t)ptr.lock().get(), ptr, "PUBLIC", "", std::chrono::seconds(0) });
	}

	void Channels::set(const uintptr_t id, const std::string& str)
	{
		Tools::AutoLock l(mtx);

		for (auto& i : chs) {
			if (i.id_i == id) {
				i.channel = str;
				return;
			}
		}
	}


	void Channels::set_nick(const uintptr_t id, const std::string& str)
	{
		Tools::AutoLock l(mtx);

		for (auto& i : chs) {
			if (i.id_i == id) {
				i.ident = str;
				return;
			}
		}
	}
	
	std::string Channels::channel_of(const uintptr_t id)
	{
		Tools::AutoLock l(mtx);
		for (const auto& i : chs) {
			if (i.id_i == id) return i.channel;
		}
		return "";
	}

	std::string Channels::nick_of(const uintptr_t id)
	{
		Tools::AutoLock l(mtx);
		for (const auto& i : chs) {
			if (i.id_i == id) return i.ident;
		}
		return "PUBLIC";
	}

	void Channels::set_typing(const uintptr_t id)
	{
		Tools::AutoLock l(mtx);
		for (auto& i : chs) {
			if (i.id_i == id) {
				i.last_typing = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch());
			}
		}
	}

	void Channels::remove(const uintptr_t id)
	{
		Tools::AutoLock l(mtx);
		for (auto i = chs.begin(); i != chs.end(); i++) {
			if (i->id_i == id) {
				chs.erase(i);
				return;
			}
		}
	}
	std::shared_ptr<Interface::Connection> Channels::get_user(const uintptr_t id)
	{
		Tools::AutoLock l(mtx);

		for (auto& i : chs) {
			if (i.id_i == id) {
				if (!i.id_ptr.expired()) return i.id_ptr.lock();
				else break;
			}
		}
		return std::shared_ptr<Interface::Connection>();
	}

	size_t Channels::amount_of_users_typing(const std::string& str)
	{
		size_t amount = 0;

		Tools::AutoLock l(mtx);

		const auto now = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch());

		for (const auto& i : chs) {
			if (i.channel == str && (now - i.last_typing <= time_typing_limit)) amount++;
		}

		return amount;
	}

	std::vector<user_info> Channels::list(const std::string& str)
	{
		std::vector<user_info> found;

		Tools::AutoLock l(mtx);

		for (const auto& i : chs) {
			if (i.channel == str) found.push_back(i);
		}

		return found;
	}
}