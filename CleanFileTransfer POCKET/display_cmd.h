#pragma once

#include <LSWv5.h>

#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <Windows.h>
#include <conio.h>
#include <functional>

using namespace LSW::v5;

namespace Custom {

	void draw_color(const int, const char);

	// simple tools console

	void go_to_xy(const int, const int);
	void clear_cmd();
	void show_console_cursor(bool);
	void disable_echo(bool);
	int get_ch();
	void resize_console_lock(const int, const int);

	template<size_t WIDTH, size_t HEIGHT>
	class CmdDisplay {
		Tools::char_c display_buf[2][HEIGHT][WIDTH];
		Tools::SuperThread<> display_thr{ Tools::superthread::performance_mode::EXTREMELY_LOW_POWER };
		size_t countdown_draws = 0;
		
		Tools::SuperMutex pos_draw_mtx;
		std::vector<std::function<bool(int, int, Tools::char_c&)>> pos_draw;
		std::vector<std::pair<size_t, std::function<void(CmdDisplay<WIDTH, HEIGHT>&)>>> pos_pos;
		size_t pos_pos_c = 0;

		void _display_thread(Tools::boolThreadF);

		bool check_coord(const int y, const int x) const;
		// y, x
		Tools::char_c& get_char_curr(const int y, const int x);
		// y, x
		Tools::char_c& get_char_buf(const int y, const int x);

		void __first_clean();
	public:
		CmdDisplay();
		~CmdDisplay();

		void set_window_name(const std::string&);

		void refresh_all_forced();
		// y, x, string
		void draw_at(const int, const int, const Tools::Cstring&, bool = true);
		// char
		void clear_all_to(const Tools::char_c);

		void wait_for_n_draws(const size_t = 2);

		// once a draw, call
		size_t add_tick_func(std::function<void(CmdDisplay<WIDTH, HEIGHT>&)>);
		// remove id got from add_tick_func
		void remove_tick_func(const size_t);

		// extra feature: identify them with return true if parameters match. X, Y, char& is pos' val
		void add_drawing_func(std::function<bool(int, int, Tools::char_c&)>);
		// if this as first param returns true, delete
		void remove_drawing_func(int);
	};

	class UserInput {
		mutable Tools::SuperMutex input_mtx;
		std::string input;
		Tools::SuperThread<> input_thread{ Tools::superthread::performance_mode::LOW_POWER };
		std::function<void(const std::string&)> input_handle;
		std::function<void(const std::string&)> input_keystroke;

		size_t limit_length = 0;
		bool ignore_stokes = true;

		void _handle_thread(Tools::boolThreadF);
	public:
		UserInput();
		~UserInput();

		void ignore_strokes(const bool);
		void limit_max_length(const size_t);

		const std::string& read_current() const;
		void on_keystroke(const std::function<void(const std::string&)>);
		void on_enter(const std::function<void(const std::string&)>);
	};
}

#include "display_cmd.ipp"