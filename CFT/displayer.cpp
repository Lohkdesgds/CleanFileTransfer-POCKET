#include "displayer.h"
#include "resource.h"

#include <allegro5/allegro_primitives.h>

const ALLEGRO_COLOR MainDisplay::mask_body = al_map_rgb(255, 0, 255);

MainDisplay::display_drawing::display_drawing() :
	m_font_resource(AllegroCPP::file_load_resource_name_to_temp_file(IDR_FONT1, "FONT_TYPE", ".ttf")),
	m_body_png_resource(AllegroCPP::file_load_resource_name_to_temp_file(IDR_PNG1, "PNG_TYPE", ".png"))
{
	font_32 = new AllegroCPP::Font(32, m_font_resource.clone_for_read());
	font_28 = new AllegroCPP::Font(28, m_font_resource.clone_for_read());
	body = new AllegroCPP::Bitmap(m_body_png_resource.clone_for_read(), 1024, 0, ".PNG");
}

MainDisplay::display_drawing::~display_drawing()
{
	delete font_32;
	delete font_28;
	delete body;
	font_28 = font_32 = nullptr;
	body = nullptr;
	std::remove(m_body_png_resource.get_filepath().c_str());
	std::remove(m_font_resource.get_filepath().c_str());
}


MainDisplay::MainDisplay() : 
	m_disp(display_size[0], display_size[1], "CleanFileTransfer Ultimate", ALLEGRO_NOFRAME), m_data(), m_draw()
{
	m_draw.font_32->set_draw_properties({
		al_map_rgb(255,255,255),
		AllegroCPP::font_multiline_b::MULTILINE
	});	
	m_draw.font_28->set_draw_properties({
		al_map_rgb(255,255,255),
		AllegroCPP::font_multiline_b::MULTILINE
	});
	m_draw.body->set_draw_properties({
		AllegroCPP::bitmap_scale{1.0f, 1.0f}
	});

	m_disp.make_window_masked(mask_body);
	al_init_primitives_addon();
}

MainDisplay::~MainDisplay()
{	
}

void MainDisplay::task_draw()
{
	auto& font32 = *m_draw.font_32;
	auto& font28 = *m_draw.font_28;
	auto& body = *m_draw.body;

	m_disp.clear_to_color();

	body.draw();

	//font32.draw(0, 30, "This is 32");
	font28.draw(188, 41, "UNDER TESTS");

	//al_draw_filled_rectangle(40, 40, 80, 80, al_map_rgb(255, 255, 0));

	m_disp.flip();
}