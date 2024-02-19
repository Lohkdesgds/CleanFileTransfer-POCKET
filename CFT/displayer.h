#pragma once

#include <AllegroCPP/include/Graphics.h>
#include <AllegroCPP/include/System.h>

class MainDisplay {
	static constexpr int display_size[2] = { 600, 800 };
	static const ALLEGRO_COLOR mask_body;

	AllegroCPP::Display m_disp;

	struct display_data {
		std::string ip_addr;
		std::string ip_port;
		double progress_bar = 0.0;
		std::vector<std::string> items_to_send;
	} m_data;

	struct display_drawing {
		AllegroCPP::File_tmp m_font_resource;
		AllegroCPP::File_tmp m_body_png_resource; 

		AllegroCPP::Font *font_32, *font_28;
		AllegroCPP::Bitmap *body;

		display_drawing();
		~display_drawing();
	} m_draw;

public:
	MainDisplay();
	~MainDisplay();

	void task_draw();

	void update_progress(double);

};