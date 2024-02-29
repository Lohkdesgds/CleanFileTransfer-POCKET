#pragma once

#include <AllegroCPP/include/Graphics.h>
#include <AllegroCPP/include/System.h>

#include "file_reader.h"
#include "file_reference.h"
#include "clickable_bitmap.h"
#include "clickable_text.h"
#include "app_props.h"
#include "resource.h"

class App {
	AllegroCPP::Display m_disp;

	// Resources
	AllegroCPP::File_tmp
		m_font_resource, // Font ttf file
		m_body_resource; // PNG file
	AllegroCPP::Bitmap
		*m_base_png; // Body
	AllegroCPP::Font
		*m_font20, // Variants of Font
		*m_font24, // Variants of Font
		*m_font28; // Variants of Font

	AllegroCPP::Event_queue 
		eq_draw, // so it always be vsync or less
		eq_think; // all tasks not related to draw

	AllegroCPP::Timer es_draw_timer{ 1.0 }, es_think_timed_stuff{ 1.0 / 60 }, es_think_timed_slow_stuff{ 1.0 / 5 };
	AllegroCPP::Event_drag_and_drop es_dnd;

	const std::vector<ClickableBase*> m_objects;
	std::vector<ItemDisplay*> m_item_list;
	std::recursive_mutex m_item_list_mtx;

	// target for text input
	ClickableText* m_selected_target_for_text = nullptr;

	// Debug info:
	ClickableText* m_top_text;
	// Show items in name
	ClickableText* m_top_list_text;

	// smooth listing
	double m_smooth_scroll = 0.0; // follows m_smooth_scroll_target
	double m_smooth_scroll_target = 0.0; // absolute pos, related to m_item_list.size()

	bool m_ipaddr_locked = false; // related to m_ipaddr
	bool m_is_host = true;
	bool m_is_send_receive_enabled = false;
	bool m_closed_flag = false;

	void think_timed();
	void think_timed_slow();

	void hint_line_set(const std::string&);
	void push_item_to_list(const std::string&);
public:
	App();
	~App();

	bool draw();
	bool think();

	bool running() const;

	std::shared_ptr<File_reference> get_next_file_to_transfer() const;
	void move_to_bottom_this_file(const std::shared_ptr<File_reference>& file_ref);
private:
	std::vector<ClickableBase*> _generate_all_items_in_screen();

	struct window_drag_wrk {
		static constexpr int grab_window_limits[2][2] = { {0, 0}, {600, 30} };

		bool holding_window = false;
		int offset_screen[2] = { 0,0 };
		
		void auto_event(const ALLEGRO_EVENT&);
		void auto_think(AllegroCPP::Display& d);
	} m_d_drag_auto;
};


//class FrontEnd {
//public:
//	enum class e_event {NONE, APP_CLOSE, POSTED_NEW_ITEMS, WANT_CONNECT, WANT_DISCONNECT};
//	enum class e_connection_status : uint8_t {DISCONNECTED, CONNECTING, CONNECTED, QUERY_DISCONNECT, DISCONNECTING};
//	enum class e_keyboard_input_targeting {NOTHING, IP_ADDR};
//	enum class e_connection_mode {HOST, CLIENT};
//private:
//	static constexpr int display_size[2] = { 600, 800 };
//	static const ALLEGRO_COLOR mask_body;
//	static constexpr int drag_and_drop_event_id = 1024;
//
//	// list of areas, made static.
//	static constexpr int close_limits[2][2] = { {569, 3}, {593, 27} };
//	static constexpr int grab_window_limits[2][2] = { {0, 0}, {600, 30} };
//	static constexpr int switch_host_client_limits[2][2] = { {189, 42}, {426, 77} };
//	static constexpr int connect_contrl_limits[2][2] = { {432, 37}, {595, 82} };
//	static constexpr int input_address_limits[2][2] = { {59, 91}, {589,125} };
//	static constexpr int items_zone_limits[2][2] = { {0, 170},{600, 800} };
//	static constexpr size_t max_address_length = 256;
//	static constexpr size_t max_items_shown_on_screen = 18;
//	static constexpr size_t item_shown_height = 35; // from bitmap, used @ many places
//	static constexpr int64_t item_sliding_text_off_px_time = 100;
//	static constexpr int max_width_text_item = 565;
//
//	AllegroCPP::Display m_disp;
//	AllegroCPP::Event_queue m_evq;
//	AllegroCPP::Event_drag_and_drop m_ev_dnd;
//	AllegroCPP::Timer m_smooth_anims{ 1.0 / 60 };
//
//	AllegroCPP::Event_queue m_screen_sync;
//	AllegroCPP::Timer m_screen_time{ 1 };
//
//	struct display_data {
//		std::string ip_addr;
//		std::string ip_port;
//		double progress_bar = 0.0;
//		std::vector<std::string> items_to_send;
//	} m_data;
//
//	struct display_work {
//		AllegroCPP::File_tmp m_font_resource;
//		AllegroCPP::File_tmp m_body_png_resource; 
//
//		AllegroCPP::Font *font_24, *font_28, *font_20;
//		AllegroCPP::Bitmap *body;
//		AllegroCPP::Bitmap x_btn[2]; // normal, clicked
//		AllegroCPP::Bitmap slider_switch_host_client; // x: 189..305
//		AllegroCPP::Bitmap connect_btn[3]; // Clickable, not clicked, hover after connected
//		AllegroCPP::Bitmap item_frame; // y=888, overlay
//		AllegroCPP::Bitmap x_btn_items[5]; // delete normal, down, up, ended good, alert
//
//		AllegroCPP::Bitmap frame_of_items_mask_target; // self created, mask target for items, then drawn
//
//		uint8_t x_btn_sel = 0; // 0..1
//		e_connection_status connect_btn_sel = e_connection_status::DISCONNECTED; // 0..1
//		int32_t slider_switch_host_client_pos_smooth = 189;
//		int32_t slider_switch_host_client_dir = -1; // 1 (client) or -1 (host).
//
//		std::string ip_addr = "localhost";
//		e_keyboard_input_targeting kb_target = e_keyboard_input_targeting::NOTHING;
//
//		size_t items_to_send_off_y = 0; // smooth to target
//		size_t items_to_send_off_y_target = 0; // power of item_shown_height
//		size_t items_to_send_temp_from_where_anim = static_cast<size_t>(-1);
//		bool update_frame_of_items_mask_target = true;
//		std::mutex items_to_send_mtx;
//		std::vector<std::shared_ptr<File_reference>> items_to_send;
//
//		display_work();
//		~display_work();
//	} m_draw;
//
//	struct delayed_mouse_axes {
//		int m[2] = { 0,0 };
//		int dz_acc = 0;
//		bool had = false;
//
//		void post(ALLEGRO_MOUSE_EVENT);
//		bool consume();
//		void reset();
//	} m_mouse_axes;
//
//	struct drag_functionality {
//		bool holding_window = false;
//		int offset_screen[2] = { 0,0 };
//		/*bool is_dragging = false;
//		int mouse_dragging_offset[2] = { 0,0 };
//
//		void apply_func_if_needed(AllegroCPP::Display&);*/
//		void move_what_is_possible(AllegroCPP::Display&);
//	} m_ev_drag_window;
//
//public:
//	FrontEnd();
//	~FrontEnd();
//
//	e_event task_events();
//	void task_draw();
//
//	e_connection_mode get_connection_mode_selected() const;
//	void set_connection_status(e_connection_status);
//
//	std::shared_ptr<File_reference> get_next_ready_to_send_ref();
//};