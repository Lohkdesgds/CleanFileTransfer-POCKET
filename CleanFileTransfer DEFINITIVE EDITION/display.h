#pragma once

#include "synced.h"

#include <stdio.h>
#include <stdlib.h>
#include <Windows.h>

#include <string>
#include <functional>
#include <vector>
#include <mutex>
#include <chrono>
#include <thread>
#include <conio.h>

void gotoxy(const short, const short);
void getcmdsize(int&, int&);
void echokb(const bool);
std::string slice_by_time(const std::string&, const size_t);

constexpr size_t max_cmd_len = filename_len;
constexpr size_t max_buf_lines = 100;
constexpr size_t max_buf_eachline_len = 256;

class DisplayCMD {
	const std::function<std::string(void)> gen_top;
	const std::function<void(const std::string&)> on_in;

	std::vector<std::string> lines;
	std::recursive_mutex saf;
	bool keep = false;
	std::string curr_input;
	std::thread thr;// , thr2;

	void draw();
	void cread();
public:
	DisplayCMD(const std::function<std::string(void)>&, const std::function<void(const std::string&)>&);
	~DisplayCMD();

	void push_message(const std::string&);

	void set_end();

	void yield_draw();
};