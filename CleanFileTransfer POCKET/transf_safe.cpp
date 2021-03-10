#include "transf_safe.h"

namespace Custom {

	bool TransferHelp::add_me_on_sender(const uintptr_t me, const uintptr_t from_who)
	{
		if (me == from_who) return false;
		Tools::AutoLock l(mtx);
		if (std::find(busy_send.begin(), busy_send.end(), from_who) != busy_send.end()) return false;

		for (auto& i : linkage) {
			if (i.first == from_who && i.second == me) return true;
		}
		linkage.push_back({ from_who, me });
		return true;
	}

	void TransferHelp::set_transfering(const uintptr_t id)
	{
		Tools::AutoLock l(mtx);
		if (std::find(busy_send.begin(), busy_send.end(), id) == busy_send.end()) busy_send.push_back(id);
	}

	/*void TransferHelp::remove_transfering(const uintptr_t id)
	{
		Tools::AutoLock l(mtx);
		if (auto pos = std::find(busy_send.begin(), busy_send.end(), id); pos != busy_send.end()) busy_send.erase(pos);
		for (auto i = linkage.begin(); i != linkage.end(); i++) {
			if (i->first == id) {
				auto cpy = i--;
				linkage.erase(cpy);
			}
		}
	}*/

	std::vector<uintptr_t> TransferHelp::list_transf(const uintptr_t sender)
	{
		std::vector<uintptr_t> vec;

		for (auto& i : linkage) {
			if (i.first == sender) vec.push_back(i.second);
		}

		return vec;
	}

	void TransferHelp::cleanup_user(const uintptr_t id)
	{
		Tools::AutoLock l(mtx);

		if (auto pos = std::find(busy_send.begin(), busy_send.end(), id); pos != busy_send.end())
			busy_send.erase(pos);

		std::vector<std::pair<uintptr_t, uintptr_t>>::iterator i_;

		while ((i_ = std::find_if(linkage.begin(), linkage.end(), [&](auto&& p) { return p.first == id || p.second == id; })) != linkage.end()) {
			linkage.erase(i_);
		}
		/*
		for (size_t p = 0; p < linkage.size(); p++) {
			
			std::unordered_map<uintptr_t, uintptr_t>::iterator i = linkage.begin() + p;

			if (i->first == id || i->second == id) {
				auto cpy = i;
				if (linkage.size() && i != linkage.begin()) i--;
				linkage.erase(cpy);
			}
		}*/
	}

}