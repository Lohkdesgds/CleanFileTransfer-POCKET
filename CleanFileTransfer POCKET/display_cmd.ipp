#include "display_cmd.h"

namespace Custom {

	void go_to_xy(const int x, const int y) {
		COORD coord{};
		coord.X = x;
		coord.Y = y;
		SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), coord);
	}

	void clear_cmd() {
		COORD topLeft = { 0, 0 };
		HANDLE console = GetStdHandle(STD_OUTPUT_HANDLE);
		CONSOLE_SCREEN_BUFFER_INFO screen;
		DWORD written;

		GetConsoleScreenBufferInfo(console, &screen);
		FillConsoleOutputCharacterA(
			console, ' ', screen.dwSize.X * screen.dwSize.Y, topLeft, &written
		);
		FillConsoleOutputAttribute(
			console, FOREGROUND_GREEN | FOREGROUND_RED | FOREGROUND_BLUE,
			screen.dwSize.X * screen.dwSize.Y, topLeft, &written
		);
		SetConsoleCursorPosition(console, topLeft);
	}

	void show_console_cursor(bool showFlag)
	{
		HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);

		CONSOLE_CURSOR_INFO     cursorInfo;

		GetConsoleCursorInfo(out, &cursorInfo);
		cursorInfo.bVisible = showFlag; // set the cursor visibility
		SetConsoleCursorInfo(out, &cursorInfo);
	}

	void disable_echo(bool disable) {

		HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);
		DWORD mode = 0;
		GetConsoleMode(hStdin, &mode);
		if (disable) SetConsoleMode(hStdin, mode & (~ENABLE_ECHO_INPUT));
		else SetConsoleMode(hStdin, mode | ENABLE_ECHO_INPUT);
	}

	int get_ch() {
		return _getch();
	}

	void resize_console_lock(const int x, const int y)
	{
		HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);

		if (h == INVALID_HANDLE_VALUE)
			throw std::runtime_error("Unable to get stdout handle.");

		// If either dimension is greater than the largest console window we can have,
		// there is no point in attempting the change.
		{
			COORD largestSize = GetLargestConsoleWindowSize(h);
			if (x > largestSize.X)
				throw std::invalid_argument("The x dimension is too large.");
			if (y > largestSize.Y)
				throw std::invalid_argument("The y dimension is too large.");
		}


		CONSOLE_SCREEN_BUFFER_INFO bufferInfo;
		if (!GetConsoleScreenBufferInfo(h, &bufferInfo))
			throw std::runtime_error("Unable to retrieve screen buffer info.");

		SMALL_RECT& winInfo = bufferInfo.srWindow;
		COORD windowSize = { winInfo.Right - winInfo.Left + 1, winInfo.Bottom - winInfo.Top + 1 };

		if (windowSize.X > x || windowSize.Y > y)
		{
			// window size needs to be adjusted before the buffer size can be reduced.
			SMALL_RECT info =
			{
				0,
				0,
				static_cast<SHORT>(x < windowSize.X ? x - 1 : windowSize.X - 1),
				static_cast<SHORT>(y < windowSize.Y ? y - 1 : windowSize.Y - 1)
			};

			if (!SetConsoleWindowInfo(h, TRUE, &info))
				throw std::runtime_error("Unable to resize window before resizing buffer.");
		}

		COORD size = { static_cast<SHORT>(x), static_cast<SHORT>(y) };
		if (!SetConsoleScreenBufferSize(h, size))
			throw std::runtime_error("Unable to resize screen buffer.");


		SMALL_RECT info = { 0, 0, static_cast<SHORT>(x - 1), static_cast<SHORT>(y - 1) };
		if (!SetConsoleWindowInfo(h, TRUE, &info))
			throw std::runtime_error("Unable to resize window after resizing buffer.");

		// lock size
		HWND cw = GetConsoleWindow();
		SetWindowLong(cw, GWL_STYLE, GetWindowLong(cw, GWL_STYLE) & ~WS_MAXIMIZEBOX & ~WS_SIZEBOX);
	}


	// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - //
	// - - - - - - - - - - - - - - - - - - CLASS - - - - - - - - - - - - - - - - - - //
	// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - //


	void draw_color(const int cl, const char ch)
	{
		HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);

		SetConsoleTextAttribute(hConsole, cl);
		putchar(ch);
	}

	template<size_t WIDTH, size_t HEIGHT>
	inline void CmdDisplay<WIDTH, HEIGHT>::_display_thread(Tools::boolThreadF b)
	{
		HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);

		while (b()) {
			Tools::AutoLock l(pos_draw_mtx);

			for (int y = 0; y < HEIGHT; y++)
			{
				for (int x = 0; x < WIDTH; x++) {

					for (auto& i : pos_draw) i(x, y, get_char_buf(y, x));

					if (get_char_curr(y, x) != get_char_buf(y, x)) {
						get_char_curr(y, x) = get_char_buf(y, x);
						go_to_xy(static_cast<int>(x), static_cast<int>(y));
						auto& cpy = get_char_curr(y, x);
						SetConsoleTextAttribute(hConsole, static_cast<WORD>(cpy.cr));
						putchar(cpy.ch);
						//if (cpy.ch != ' ') std::this_thread::sleep_for(std::chrono::milliseconds(4));
					}
				}
			}

			for (auto& i : pos_pos) i.second(*this);

			if (countdown_draws) countdown_draws--;
		}
	}

	template<size_t WIDTH, size_t HEIGHT>
	inline bool CmdDisplay<WIDTH, HEIGHT>::check_coord(const int y, const int x) const
	{
		return y < HEIGHT && x < WIDTH && y >= 0 && x >= 0;
	}


	template<size_t WIDTH, size_t HEIGHT>
	inline Tools::char_c& CmdDisplay<WIDTH, HEIGHT>::get_char_curr(const int y, const int x)
	{
		return display_buf[0][y][x];
	}

	template<size_t WIDTH, size_t HEIGHT>
	inline Tools::char_c& CmdDisplay<WIDTH, HEIGHT>::get_char_buf(const int y, const int x)
	{
		return display_buf[1][y][x];
	}

	template<size_t WIDTH, size_t HEIGHT>
	inline void CmdDisplay<WIDTH, HEIGHT>::__first_clean()
	{
		for (int y = 0; y < HEIGHT; y++)
		{
			for (int x = 0; x < WIDTH; x++) {
				get_char_curr(y, x) = { '#' };
				get_char_buf(y, x) = { ' ' };
			}
		}
	}

	template<size_t WIDTH, size_t HEIGHT>
	inline CmdDisplay<WIDTH, HEIGHT>::CmdDisplay()
	{
		disable_echo(true);
		show_console_cursor(false);
		resize_console_lock(WIDTH + 1, HEIGHT);
		__first_clean();
		display_thr.set([&](Tools::boolThreadF b) {_display_thread(b);});
		display_thr.start();
	}

	template<size_t WIDTH, size_t HEIGHT>
	inline CmdDisplay<WIDTH, HEIGHT>::~CmdDisplay()
	{
		display_thr.stop();
		display_thr.join();
	}

	template<size_t WIDTH, size_t HEIGHT>
	inline void CmdDisplay<WIDTH, HEIGHT>::set_window_name(const std::string& str)
	{
		SetConsoleTitle(str.c_str());
	}

	template<size_t WIDTH, size_t HEIGHT>
	inline void CmdDisplay<WIDTH, HEIGHT>::refresh_all_forced()
	{
		for (int y = 0; y < HEIGHT; y++)
		{
			for (int x = 0; x < WIDTH; x++)
			{
				get_char_curr(y, x) = { get_char_buf(y, x).ch + 1, get_char_buf(y, x).cr };
			}
		}
	}

	template<size_t WIDTH, size_t HEIGHT>
	inline void CmdDisplay<WIDTH, HEIGHT>::draw_at(const int y, const int x, const Tools::Cstring& str, bool auto_slide)
	{
		const auto max_len = WIDTH - x;
		int posx = x;

		if (auto_slide) {
			Tools::Cstring to_draw = str.substr(str.size() > max_len ? str.size() - max_len : 0, max_len);

			for (auto& i : to_draw) {
				if (check_coord(y, posx)) get_char_buf(y, posx) = i;
				else break;
				posx++;
			}
		}
		else {
			for (auto& i : str) {
				if (check_coord(y, posx)) get_char_buf(y, posx) = i;
				else break;
				posx++;
			}
		}

	}

	template<size_t WIDTH, size_t HEIGHT>
	inline void CmdDisplay<WIDTH, HEIGHT>::clear_all_to(const Tools::char_c ch)
	{
		for (int y = 0; y < HEIGHT; y++)
		{
			for (int x = 0; x < WIDTH; x++)
			{
				get_char_buf(y, x) = ch;
			}
		}
	}

	template<size_t WIDTH, size_t HEIGHT>
	inline void CmdDisplay<WIDTH, HEIGHT>::wait_for_n_draws(const size_t n)
	{
		countdown_draws = n;
		while (countdown_draws) std::this_thread::yield();
	}

	template<size_t WIDTH, size_t HEIGHT>
	inline size_t CmdDisplay<WIDTH, HEIGHT>::add_tick_func(std::function<void(CmdDisplay<WIDTH, HEIGHT>&)> f)
	{
		if (!f) return static_cast<size_t>(-1);
		Tools::AutoLock l(pos_draw_mtx);
		size_t nowc = pos_pos_c++;
		if (!pos_pos_c) nowc = pos_pos_c++; // ensure no -1.
		pos_pos.push_back({ nowc, f });
		return nowc;
	}

	template<size_t WIDTH, size_t HEIGHT>
	inline void CmdDisplay<WIDTH, HEIGHT>::remove_tick_func(const size_t id)
	{
		Tools::AutoLock l(pos_draw_mtx);
		for (auto i = pos_pos.begin(); i != pos_pos.end(); i++) {
			if (i->first == id) {
				pos_pos.erase(i);
				break;
			}
		}
	}

	template<size_t WIDTH, size_t HEIGHT>
	inline void CmdDisplay<WIDTH, HEIGHT>::add_drawing_func(std::function<bool(int, int, Tools::char_c&)> f)
	{
		Tools::AutoLock l(pos_draw_mtx);
		pos_draw.push_back(f);
	}

	template<size_t WIDTH, size_t HEIGHT>
	inline void CmdDisplay<WIDTH, HEIGHT>::remove_drawing_func(int a)
	{
		Tools::char_c buf;
		Tools::AutoLock l(pos_draw_mtx);
		for (auto i = pos_draw.begin(); i != pos_draw.end(); i++) {
			if ((*i)(a, 0, buf)) {
				pos_draw.erase(i);
				break;
			}
		}
	}

	inline void UserInput::_handle_thread(Tools::boolThreadF b)
	{
		while (b()) {
			if (ignore_stokes) {
				while (_kbhit()) get_ch();
				std::this_thread::sleep_for(std::chrono::milliseconds(100));
				continue;
			}

			if (!_kbhit()) {
				std::this_thread::sleep_for(std::chrono::milliseconds(100));
				continue;
			}

			char ch = get_ch();
			size_t send_empty_string = 0;

			Tools::AutoLock l(input_mtx);

			if (ch == 13) {
				if (input_handle) input_handle(limit_length ? input.substr(0, limit_length) : input);
				send_empty_string = input.length();
				input.clear();
			}
			else if (ch == 8) {
				if (input.length()) input.pop_back();
			}
			else {
				if (!limit_length || input.length() < limit_length) input += ch;
			}
			if (input_keystroke) {
				if (send_empty_string) {
					std::string temp(send_empty_string, ' ');
					input_keystroke(limit_length ? temp.substr(0, limit_length) : temp);
					input_keystroke(limit_length ? input.substr(0, limit_length) : input);
				}
				else input_keystroke(limit_length ? input.substr(0, limit_length) : input);
			}
		}
	}

	inline UserInput::UserInput()
	{
		input_thread.set([&](Tools::boolThreadF b) {_handle_thread(b); });
		input_thread.start();
	}

	inline UserInput::~UserInput()
	{
		input_thread.stop();
	}

	inline void UserInput::ignore_strokes(const bool b)
	{
		ignore_stokes = b;
	}

	inline void UserInput::limit_max_length(const size_t s)
	{
		limit_length = s;
	}

	inline const std::string& UserInput::read_current() const
	{
		Tools::AutoLock l(input_mtx);
		return input;
	}

	inline void UserInput::on_keystroke(const std::function<void(const std::string&)> f)
	{
		Tools::AutoLock l(input_mtx);
		input_keystroke = f;
	}

	inline void UserInput::on_enter(const std::function<void(const std::string&)> f)
	{
		Tools::AutoLock l(input_mtx);
		input_handle = f;
	}
}