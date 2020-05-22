#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <Windows.h>
#include <conio.h>
#include <functional>

namespace Custom {

	// simple tools console

	void gotoxy(const int x, const int y) {
		COORD coord;
		coord.X = x;
		coord.Y = y;
		SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), coord);
	}
	void clscmd() {
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
	void ShowConsoleCursor(bool showFlag)
	{
		HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);

		CONSOLE_CURSOR_INFO     cursorInfo;

		GetConsoleCursorInfo(out, &cursorInfo);
		cursorInfo.bVisible = showFlag; // set the cursor visibility
		SetConsoleCursorInfo(out, &cursorInfo);
	}
	void disableEcho(bool disable) {

		HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);
		DWORD mode = 0;
		GetConsoleMode(hStdin, &mode);
		if (disable) SetConsoleMode(hStdin, mode & (~ENABLE_ECHO_INPUT));
		else SetConsoleMode(hStdin, mode | ENABLE_ECHO_INPUT);
	}
	/*int transfDoubleChar(const char a, const char b)
	{
		return ((int)b + ((int)a << 8));
	}*/
	int getCH() {
		/*char buf[2];
		buf[0] = _getch();
		buf[1] = _getch();
		return transfDoubleChar(buf[0], buf[1]);*/
		return _getch();
	}


	// const

	const ULONGLONG clear_screen_at = static_cast<ULONGLONG>(10e4);
	const unsigned max_char_update_per_frame = 30;

	// class

	template<size_t X, size_t Y>
	class DISPLAY {
		char matrix[X][Y];
		char matrix_last[X][Y];
		const size_t line_count = Y - 8;

		ULONGLONG latest_clear = 0;
		std::mutex m;

		std::function<std::string(void)> title_f, line_f[Y - 8];


		void setAt(const int, const int, const char);
		void _setTitle(const std::string);
		void _printAt(const int, const std::string);
	public:
		DISPLAY();

		void setTitle(const std::string);
		void setTitle(const std::function<std::string(void)>);
		void setCommandEntry(const std::string);

		size_t getLineAmount();
		void clearAllLines();
		void printAt(const int, const std::string);
		void printAt(const int, const std::function<std::string(void)>);
		void flip();

		void newMessage(const std::string);
	};


	template<size_t X, size_t Y>
	inline void DISPLAY<X, Y>::setAt(const int x, const int y, const char c)
	{
		matrix[x][y] = c;
	}

	template<size_t X, size_t Y>
	inline void DISPLAY<X, Y>::_setTitle(const std::string str)
	{
		for (size_t p = 0; p < X; p++) {
			if (p < str.length()) setAt(static_cast<int>(p), 1, str[p]);
			else setAt(static_cast<int>(p), 1, ' ');
		}
	}

	template<size_t X, size_t Y>
	inline void DISPLAY<X, Y>::_printAt(const int line_c, const std::string str)
	{
		if (line_c < line_count && line_c >= 0) {
			for (size_t p = 0; p < X; p++) {
				if (p < str.length()) setAt(static_cast<int>(p), line_c + 3, str[p]);
				else setAt(static_cast<int>(p), line_c + 3, ' ');
			}
		}
	}

	template<size_t X, size_t Y>
	inline DISPLAY<X, Y>::DISPLAY()
	{
		for (auto& i : matrix_last) for (auto& j : i) j = ' ';
		for (auto& i : matrix) for(auto& j : i) j = ' ';

		for (int x = 0; x < static_cast<int>(X); x++)
		{
			setAt(x, 0, '#');
			setAt(x, 2, '#');
			setAt(x, Y - 4, '#');
		}
		setAt(0, Y - 5, '>');
	}

	template<size_t X, size_t Y>
	inline void DISPLAY<X, Y>::setTitle(const std::string str)
	{
		m.lock();
		_setTitle(str);
		title_f = std::function<std::string(void)>();
		m.unlock();
	}

	template<size_t X, size_t Y>
	inline void DISPLAY<X, Y>::setTitle(const std::function<std::string(void)> f)
	{
		m.lock();
		title_f = f;
		m.unlock();
	}

	template<size_t X, size_t Y>
	inline void DISPLAY<X, Y>::setCommandEntry(const std::string str)
	{
		m.lock();
		for (int p = 2; p < static_cast<int>(X); p++) {
			if (static_cast<size_t>(p) - 2 < str.length()) setAt(p, Y - 5, str[static_cast<size_t>(p) - 2]);
			else setAt(p, Y - 5, ' ');
		}
		m.unlock();
	}

	template<size_t X, size_t Y>
	inline size_t DISPLAY<X, Y>::getLineAmount()
	{
		return line_count;
	}

	template<size_t X, size_t Y>
	inline void DISPLAY<X, Y>::clearAllLines()
	{
		for (int u = 0; u < getLineAmount(); u++) {
			printAt(u, "");
		}
	}

	template<size_t X, size_t Y>
	inline void DISPLAY<X, Y>::printAt(const int line_c, const std::string str)
	{
		m.lock();
		if (line_c < line_count && line_c >= 0) {
			line_f[line_c] = std::function<std::string(void)>();
			_printAt(line_c, str);
		}
		m.unlock();
	}

	template<size_t X, size_t Y>
	inline void DISPLAY<X, Y>::printAt(const int line_c, const std::function<std::string(void)> f)
	{
		m.lock();
		if (line_c < line_count && line_c >= 0) {
			line_f[line_c] = f;			
		}
		m.unlock();
	}

	template<size_t X, size_t Y>
	inline void DISPLAY<X, Y>::flip()
	{
		if (GetTickCount64() - latest_clear > clear_screen_at) {
			latest_clear = GetTickCount64();
			for (auto& i : matrix_last) for (auto& j : i) j = ' ';
			m.lock();
			clscmd();
			m.unlock();
		}

		if (title_f) {
			std::string newt = title_f();
			_setTitle(newt);
		}
		for (size_t lc = 0; lc < line_count; lc++) {
			auto& i = line_f[lc];
			if (i) {
				std::string ll = i();
				_printAt(static_cast<int>(lc), ll);
			}
		}

		m.lock();

		size_t max_changes_per_frame = 0;

		for (size_t y = 0; y < Y; y++) {
			for (size_t x = 0; x < X; x++) {
				if (matrix_last[x][y] != matrix[x][y]) {
					gotoxy(static_cast<int>(x), static_cast<int>(y));
					putchar(matrix[x][y]);
					matrix_last[x][y] = matrix[x][y];
					if (++max_changes_per_frame == max_char_update_per_frame) {
						m.unlock();
						return;
					}
				}
			}
		}
		m.unlock();
	}

	template<size_t X, size_t Y>
	inline void DISPLAY<X, Y>::newMessage(const std::string str)
	{
		m.lock();
		for (size_t y = Y - 3; y < Y - 1; y++) {
			for (size_t x = 0; x < X; x++) {
				matrix[x][y] = matrix[x][y+1];
			}
		}
		for (size_t p = 0; p < X; p++) {
			if (p < str.length()) setAt(static_cast<int>(p), static_cast<int>(Y) - 1, str[p]);
			else setAt(static_cast<int>(p), Y - 1, ' ');
		}
		m.unlock();

	}

}