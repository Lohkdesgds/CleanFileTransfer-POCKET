#pragma once

#include <LSWv5.h>

using namespace LSW::v5;

namespace Custom {

	class MultiLiner {
		Tools::SuperMutex mtx;
		std::vector<Tools::Cstring> lines;
		size_t limit_lines = 0, width_fixed = 0;
		std::function<void(const int, const Tools::Cstring&)> work;

		void _force_update_nolock();
	public:
		void set_line_amount(const size_t);
		void set_line_width_max(const size_t);

		//void force_update();

		void clear();

		void push_back(const Tools::Cstring&);

		MultiLiner& operator<<(const Tools::Cstring&);

		void set_func_draw(std::function<void(const int, const Tools::Cstring&)>);
	};
}