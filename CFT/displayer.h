#pragma once

#include <AllegroCPP/include/Graphics.h>
#include <AllegroCPP/include/System.h>

#include "file_reference.h"

class FrontEnd {
public:
	enum class e_event {NONE, APP_CLOSE};
	enum class e_connection_status : uint8_t {DISCONNECTED, CONNECTING, CONNECTED, QUERY_DISCONNECT, DISCONNECTING};
	enum class e_keyboard_input_targeting {NOTHING, IP_ADDR};
private:
	static constexpr int display_size[2] = { 600, 800 };
	static const ALLEGRO_COLOR mask_body;
	static constexpr int drag_and_drop_event_id = 1024;

	// list of areas, made static.
	static constexpr int close_limits[2][2] = { {569, 3}, {593, 27} };
	static constexpr int grab_window_limits[2][2] = { {0, 0}, {600, 30} };
	static constexpr int switch_host_client_limits[2][2] = { {189, 42}, {426, 77} };
	static constexpr int connect_contrl_limits[2][2] = { {432, 37}, {595, 82} };
	static constexpr int input_address_limits[2][2] = { {59, 91}, {589,125} };
	static constexpr size_t max_address_length = 256;

	AllegroCPP::Display m_disp;
	AllegroCPP::Event_queue m_evq;
	AllegroCPP::Event_drag_and_drop m_ev_dnd;

	struct display_data {
		std::string ip_addr;
		std::string ip_port;
		double progress_bar = 0.0;
		std::vector<std::string> items_to_send;
	} m_data;

	struct display_work {
		AllegroCPP::File_tmp m_font_resource;
		AllegroCPP::File_tmp m_body_png_resource; 

		AllegroCPP::Font *font_24, *font_28;
		AllegroCPP::Bitmap *body;
		AllegroCPP::Bitmap x_btn[2]; // normal, clicked
		AllegroCPP::Bitmap slider_switch_host_client; // x: 189..305
		AllegroCPP::Bitmap connect_btn[3]; // Clickable, not clicked, hover after connected
		AllegroCPP::Bitmap item_frame; // y=888, overlay

		uint8_t x_btn_sel = 0; // 0..1
		e_connection_status connect_btn_sel = e_connection_status::DISCONNECTED; // 0..1
		int32_t slider_switch_host_client_pos_smooth = 189;
		int32_t slider_switch_host_client_dir = -1; // 1 or -1.

		std::string ip_addr = "localhost";
		e_keyboard_input_targeting kb_target = e_keyboard_input_targeting::NOTHING;

		size_t items_to_send_off_y = 0;
		std::vector<File_reference> items_to_send;

		display_work();
		~display_work();
	} m_draw;

	struct drag_functionality {
		bool is_dragging = false;
		int mouse_dragging_offset[2] = { 0,0 };

		void apply_func_if_needed(AllegroCPP::Display&);
	} m_ev_drag_window;

public:
	FrontEnd();
	~FrontEnd();

	e_event task_events();
	void task_draw();

	void update_progress(double);

};