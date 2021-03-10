#include "multiline_console.h"

namespace Custom {

	void MultiLiner::_force_update_nolock()
	{
		std::string empty(width_fixed + 1, ' ');

		if (work) {
			for (size_t p = 0; p < lines.size(); p++) {
				work(static_cast<int>(p), width_fixed ? (lines[p] + empty).substr(0, width_fixed) : lines[p]);
			}
		}
	}

	void MultiLiner::set_line_amount(const size_t na)
	{
		Tools::AutoLock l(mtx);
		limit_lines = na;
	}

	void MultiLiner::set_line_width_max(const size_t lf)
	{
		width_fixed = lf;
	}

	/*void MultiLiner::force_update()
	{
		Tools::AutoLock l(mtx);
		_force_update_nolock();
	}*/

	void MultiLiner::clear()
	{
		Tools::AutoLock l(mtx);
		lines.clear();
	}

	void MultiLiner::push_back(const Tools::Cstring& str)
	{
		Tools::AutoLock l(mtx);
		while (lines.size() > limit_lines) lines.erase(lines.begin());

		for (size_t p = 0; p < str.size(); p += width_fixed) {
			lines.push_back(str.substr(p, width_fixed));
		}

		while (lines.size() > (limit_lines + 1)) lines.erase(lines.begin());

		_force_update_nolock();
	}

	MultiLiner& MultiLiner::operator<<(const Tools::Cstring& str)
	{
		push_back(str);
		return *this;
	}

	void MultiLiner::set_func_draw(std::function<void(const int, const Tools::Cstring&)> wrk)
	{
		Tools::AutoLock l(mtx);
		work = wrk;
	}
}