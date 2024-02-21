#include "displayer.h"
#include "resource.h"

#include <allegro5/allegro_primitives.h>

const ALLEGRO_COLOR FrontEnd::mask_body = al_map_rgb(255, 0, 255);

FrontEnd::display_work::display_work() :
	m_font_resource(AllegroCPP::file_load_resource_name_to_temp_file(IDR_FONT1, "FONT_TYPE", ".ttf")),
	m_body_png_resource(AllegroCPP::file_load_resource_name_to_temp_file(IDR_PNG1, "PNG_TYPE", ".png"))
{
	font_24 = new AllegroCPP::Font(24, m_font_resource.clone_for_read());
	font_28 = new AllegroCPP::Font(27, 28, m_font_resource.clone_for_read());
	body = new AllegroCPP::Bitmap(m_body_png_resource.clone_for_read(), 1024, 0, ".PNG");
}

FrontEnd::display_work::~display_work()
{
	delete font_24;
	delete font_28;
	delete body;
	font_28 = font_24 = nullptr;
	body = nullptr;
	std::remove(m_body_png_resource.get_filepath().c_str());
	std::remove(m_font_resource.get_filepath().c_str());
}

void FrontEnd::delayed_mouse_axes::post(ALLEGRO_MOUSE_EVENT ev)
{
	//me = ev;
	m[0] = ev.x;
	m[1] = ev.y;
	dz_acc += ev.dz;
	had = true;
}

bool FrontEnd::delayed_mouse_axes::consume()
{
	if (!had) return false;
	had = false;
	return true;
}

void FrontEnd::delayed_mouse_axes::reset()
{
	dz_acc = 0;
}

void FrontEnd::drag_functionality::move_what_is_possible(AllegroCPP::Display& d)
{
	const int to_move_x = AllegroCPP::Mouse_cursor::get_pos_x() - offset_screen[0];
	const int to_move_y = AllegroCPP::Mouse_cursor::get_pos_y() - offset_screen[1];

	int curr_x = 0, curr_y = 0;
	d.get_position(curr_x, curr_y);

	curr_x += to_move_x;
	curr_y += to_move_y;
	offset_screen[0] += to_move_x;
	offset_screen[1] += to_move_y;

	d.set_position(curr_x, curr_y);

}


FrontEnd::FrontEnd() : 
	m_disp(display_size[0], display_size[1], "CleanFileTransfer Ultimate", ALLEGRO_NOFRAME),
	m_evq(),
	m_ev_dnd(m_disp, drag_and_drop_event_id),
	m_data(),
	m_draw()
{
	m_evq << m_disp;
	m_evq << m_ev_dnd;
	m_evq << AllegroCPP::Event_mouse();
	m_evq << AllegroCPP::Event_keyboard();
	m_evq << m_smooth_anims;

	m_draw.x_btn[0] = AllegroCPP::Bitmap(*m_draw.body, 573, 800, 27, 27);
	m_draw.x_btn[1] = AllegroCPP::Bitmap(*m_draw.body, 573, 827, 27, 27);
	m_draw.x_btn_items = AllegroCPP::Bitmap(*m_draw.body, 573, 800, 27, 27);
	m_draw.connect_btn[0] = AllegroCPP::Bitmap(*m_draw.body, 0, 845, 163, 43);
	m_draw.connect_btn[1] = AllegroCPP::Bitmap(*m_draw.body, 0, 801, 163, 43);
	m_draw.connect_btn[2] = AllegroCPP::Bitmap(*m_draw.body, 161, 801, 163, 43);
	m_draw.slider_switch_host_client = AllegroCPP::Bitmap(*m_draw.body, 452, 800, 121, 35);
	m_draw.item_frame = AllegroCPP::Bitmap(*m_draw.body, 0, 888, 600, item_shown_height);

	m_draw.frame_of_items_mask_target = AllegroCPP::Bitmap(600, 629);

	m_draw.font_24->set_draw_properties({
		al_map_rgb(255,255,255),
		AllegroCPP::font_multiline_b::MULTILINE
	});	
	m_draw.font_28->set_draw_properties({
		al_map_rgb(255,255,255),
		AllegroCPP::font_multiline_b::MULTILINE
	});
	//m_draw.body->set_draw_properties({
	//	AllegroCPP::bitmap_scale{1.0f, 1.0f}
	//});

	m_draw.x_btn[0].set_draw_properties({ AllegroCPP::bitmap_position_and_flags{572, 2, 0} });
	m_draw.x_btn[1].set_draw_properties({ AllegroCPP::bitmap_position_and_flags{572, 2, 0} });
	m_draw.slider_switch_host_client.set_draw_properties({ AllegroCPP::bitmap_position_and_flags{189, 41, 0} }); // can do 116 to right
	m_draw.connect_btn[0].set_draw_properties({ AllegroCPP::bitmap_position_and_flags{432, 37, 0} });
	m_draw.connect_btn[1].set_draw_properties({ AllegroCPP::bitmap_position_and_flags{432, 37, 0} });
	m_draw.connect_btn[2].set_draw_properties({ AllegroCPP::bitmap_position_and_flags{432, 37, 0} });

	m_draw.frame_of_items_mask_target.set_draw_properties({ AllegroCPP::bitmap_position_and_flags{0, 171, 0} });

	m_disp.make_window_masked(mask_body);
	m_smooth_anims.start();
}

FrontEnd::~FrontEnd()
{	
}

constexpr bool is_point_in(const int p[2], const int l[2][2])
{
	return
		p[0] >= l[0][0] && p[0] <= l[1][0] &&
		p[1] >= l[0][1] && p[1] <= l[1][1];
}

FrontEnd::e_event FrontEnd::task_events()
{
	if (!m_evq.has_event()) return e_event::NONE;

	auto ev = m_evq.get_next_event();
	if (!ev.valid()) return e_event::NONE;

	const auto& evr = ev.get();


	switch (evr.type) {
	case ALLEGRO_EVENT_TIMER: // smooth animations!
	{
		// main slider anim
		if (m_draw.slider_switch_host_client_dir == 1) {
			if (m_draw.slider_switch_host_client_pos_smooth < 306) {
				m_draw.slider_switch_host_client_pos_smooth += (m_draw.slider_switch_host_client_pos_smooth < 286) ? 5 : 1;
				m_draw.slider_switch_host_client.set_draw_property(AllegroCPP::bitmap_position_and_flags{ static_cast<float>(m_draw.slider_switch_host_client_pos_smooth), 41, 0 });
			}
		}
		else {
			if (m_draw.slider_switch_host_client_pos_smooth > 189) {
				m_draw.slider_switch_host_client_pos_smooth -= (m_draw.slider_switch_host_client_pos_smooth > 209) ? 5 : 1;
				m_draw.slider_switch_host_client.set_draw_property(AllegroCPP::bitmap_position_and_flags{ static_cast<float>(m_draw.slider_switch_host_client_pos_smooth), 41, 0 });
			}
		}
		// item slider y axis move
		{
			const size_t factor = 1 + ((m_draw.items_to_send_off_y < m_draw.items_to_send_off_y_target ?
				m_draw.items_to_send_off_y_target - m_draw.items_to_send_off_y : m_draw.items_to_send_off_y - m_draw.items_to_send_off_y_target)) / 8;

			if (m_draw.items_to_send_off_y < m_draw.items_to_send_off_y_target)
			{
				m_draw.items_to_send_off_y += factor;
			}
			else if (m_draw.items_to_send_off_y > m_draw.items_to_send_off_y_target) 
			{
				m_draw.items_to_send_off_y -= factor;
			}
			else{
				m_draw.items_to_send_temp_from_where_anim = static_cast<size_t>(-1); // no anim left
			}
		}
		// always update bitmap once in a while for smooth stuff
		m_draw.update_frame_of_items_mask_target = true;

		// mouse axes delayed event for easier CPU usage
		if (m_mouse_axes.consume())
		{
			if (m_ev_drag_window.holding_window) {
				m_ev_drag_window.move_what_is_possible(m_disp);
			}

			const auto& mouse_axis = m_mouse_axes.m;
			const auto& mouse_roll_positive_goes_up = m_mouse_axes.dz_acc;

			if (m_draw.connect_btn_sel == e_connection_status::QUERY_DISCONNECT) m_draw.connect_btn_sel = e_connection_status::CONNECTED;

			if (is_point_in(mouse_axis, connect_contrl_limits)) {
				if (m_draw.connect_btn_sel == e_connection_status::CONNECTED) m_draw.connect_btn_sel = e_connection_status::QUERY_DISCONNECT;
			}
			else if (mouse_roll_positive_goes_up != 0 && is_point_in(mouse_axis, items_zone_limits)) {
				m_draw.update_frame_of_items_mask_target = true;
				m_draw.items_to_send_temp_from_where_anim = static_cast<size_t>(-1); // cancel anim

				const auto real_idx = (m_draw.items_to_send_off_y_target / item_shown_height);

				if (mouse_roll_positive_goes_up > 0 && real_idx > 0) m_draw.items_to_send_off_y_target -= item_shown_height;
				if (mouse_roll_positive_goes_up < 0 && m_draw.items_to_send.size() - real_idx > max_items_shown_on_screen) m_draw.items_to_send_off_y_target += item_shown_height;
			}

			m_mouse_axes.reset();
		}
	}
		break;
	case ALLEGRO_EVENT_DISPLAY_CLOSE:
		return e_event::APP_CLOSE;
	case ALLEGRO_EVENT_MOUSE_BUTTON_DOWN:
	{
		const int mouse_axis[2] = { evr.mouse.x, evr.mouse.y };

		m_draw.x_btn_sel = 0;

		if (is_point_in(mouse_axis, close_limits)) {
			m_draw.x_btn_sel = 1;
		}
		else if (is_point_in(mouse_axis, grab_window_limits)) {
			if (!m_ev_drag_window.holding_window) {
				m_ev_drag_window.holding_window = true;
				m_ev_drag_window.offset_screen[0] = AllegroCPP::Mouse_cursor::get_pos_x();
				m_ev_drag_window.offset_screen[1] = AllegroCPP::Mouse_cursor::get_pos_y();
				//printf_s("GET %d %d\n", m_ev_drag_window.offset_screen[0], m_ev_drag_window.offset_screen[1]);
			}
		}
		else if (is_point_in(mouse_axis, switch_host_client_limits)) {
			m_draw.slider_switch_host_client_dir = m_draw.slider_switch_host_client_dir < 0 ? 1 : -1;
		}
		else if (is_point_in(mouse_axis, connect_contrl_limits)) {
			uint8_t tmp = static_cast<uint8_t>(m_draw.connect_btn_sel);
			tmp = (tmp + 1) % 5;
			m_draw.connect_btn_sel = static_cast<e_connection_status>(tmp);
		}
		else { // in items list now
			for (size_t bb = 0; bb < max_items_shown_on_screen + 1; ++bb)
			{
				const size_t expected_p = (m_draw.items_to_send_off_y / item_shown_height) + bb;
				if (expected_p >= m_draw.items_to_send.size()) break;

				auto& it = m_draw.items_to_send[expected_p];

				const int offy = it.get_collision_now_for_delete_top_y();
				const int test[2][2] = { {574, offy}, {600, offy + 27} };

				if (is_point_in(mouse_axis, test)) {
					m_draw.items_to_send.erase(m_draw.items_to_send.begin() + expected_p);

					if (m_draw.items_to_send_off_y_target >= item_shown_height) {
						m_draw.items_to_send_off_y_target -= item_shown_height;
						m_draw.items_to_send_temp_from_where_anim = expected_p;
					}
				}
			}
		}

		// keyboard related
		if (is_point_in(mouse_axis, input_address_limits)) {
			m_draw.kb_target = e_keyboard_input_targeting::IP_ADDR;
		}
		else {
			m_draw.kb_target = e_keyboard_input_targeting::NOTHING;
		}
	}
		break;
	case ALLEGRO_EVENT_MOUSE_BUTTON_UP:
	{
		m_ev_drag_window.holding_window = false;

		m_draw.x_btn_sel = 0;

		const int mouse_axis[2] = { evr.mouse.x, evr.mouse.y };

		if (is_point_in(mouse_axis, close_limits)) return e_event::APP_CLOSE;
	}
		break;
	case ALLEGRO_EVENT_MOUSE_AXES:
	{
		// moved to timer for easier consume
		m_mouse_axes.post(evr.mouse);
	}
		break;
	case ALLEGRO_EVENT_KEY_CHAR:
	{
		const int key = evr.keyboard.unichar;

		switch (evr.keyboard.keycode) {
		case ALLEGRO_KEY_ESCAPE:
		case ALLEGRO_KEY_ENTER:
		case ALLEGRO_KEY_PAD_ENTER:
			m_draw.kb_target = e_keyboard_input_targeting::NOTHING;
			break;
		case ALLEGRO_KEY_BACKSPACE:
			if (m_draw.ip_addr.length() > 0) m_draw.ip_addr.pop_back();
			break;
		default:
			if (key > 0 && (isalnum(key) || key == ':' || key == '.') && m_draw.ip_addr.length() < max_address_length) m_draw.ip_addr += key;
			break;
		}
	}
		break;
	case drag_and_drop_event_id:
	{
		m_draw.update_frame_of_items_mask_target = true;
		AllegroCPP::Drop_event ednd(evr);
		m_draw.items_to_send.push_back(ednd.c_str());
	}
		break;
	}
	return e_event::NONE;
}

void FrontEnd::task_draw()
{
	auto& font24 = *m_draw.font_24;
	auto& font28 = *m_draw.font_28;
	auto& body = *m_draw.body;

	m_disp.clear_to_color();
	body.draw();

	m_draw.x_btn[m_draw.x_btn_sel].draw();
	switch (m_draw.connect_btn_sel) {
	case e_connection_status::DISCONNECTED:
	case e_connection_status::CONNECTED:
		m_draw.connect_btn[0].draw();
		break;
	case e_connection_status::CONNECTING:
	case e_connection_status::DISCONNECTING:
		m_draw.connect_btn[1].draw();
		break;
	case e_connection_status::QUERY_DISCONNECT:
		m_draw.connect_btn[2].draw();
		break;
	}
	m_draw.slider_switch_host_client.draw();

	// top part

	font28.draw(8, -2, "CleanFileTransfer Ultimate"); // orig: 8, 4. Got: 8, -2. Assume font -= 6

	font28.draw(16, 42, "Transfer role:");
	font24.draw(218, 44, "HOST");
	font24.draw(324, 44, "CLIENT");

	switch (m_draw.connect_btn_sel) {
	case e_connection_status::DISCONNECTED:
		font28.draw(477, 42, "Start!");
		break;
	case e_connection_status::CONNECTING:
	case e_connection_status::DISCONNECTING:
		font28.draw(452, 42, "Working...");
		break;
	case e_connection_status::CONNECTED:
		font28.draw(447, 42, "Connected!");
		break;
	case e_connection_status::QUERY_DISCONNECT:
		font28.draw(473, 42, "Close?");
		break;
	}

	font28.draw(16, 90, "IP:");
	{
		auto str = m_draw.ip_addr + (m_draw.kb_target == e_keyboard_input_targeting::IP_ADDR && m_draw.ip_addr.length() < max_address_length ? (static_cast<uint64_t>(al_get_time() * 2) % 2 == 0 ? "_" : " ") : "");
		const auto width = font24.get_width(str); // if > 529, x = 588
		if (width >= 529) {
			font24.set_draw_property(AllegroCPP::text_alignment::RIGHT);

			while (font24.get_width(str) >= 529) str.erase(str.begin());

			font24.draw(588, 92, str);
			font24.set_draw_property(AllegroCPP::text_alignment::LEFT);
		}
		else {
			font24.draw(64, 92, str);
		}
	}

	// lower part

	font24.draw(8, 139, "Items to send/receive (drag and drop, auto upload!)");
	m_draw.frame_of_items_mask_target.draw();
	
	if (m_draw.update_frame_of_items_mask_target) {
		m_draw.update_frame_of_items_mask_target = false;

		m_draw.frame_of_items_mask_target.set_as_target();

		// quick transform to alpha:
		al_clear_to_color(al_map_rgb(0, 0, 0));
		al_convert_mask_to_alpha(m_draw.frame_of_items_mask_target, al_map_rgb(0, 0, 0));
		

		for (size_t bb = 0; bb < max_items_shown_on_screen + 1; ++bb)
		{
			const size_t expected_p = (m_draw.items_to_send_off_y / item_shown_height) + bb;
			const float off_y = m_draw.items_to_send_off_y % item_shown_height;

			if (expected_p >= m_draw.items_to_send.size()) break;

			auto& it = m_draw.items_to_send[expected_p];
			const auto& path = it.get_path();
			const auto width = font24.get_width(path);
			const auto factor = width > 590 ? 590 - width : 0;
			const float smooth_mult =  m_draw.items_to_send_temp_from_where_anim != static_cast<size_t>(-1) ? (expected_p < m_draw.items_to_send_temp_from_where_anim ? 1.0f : 0.0f) : 1.0f;

			font24.draw(
				8,
				4 + 70 * bb - off_y * smooth_mult,
				"#" + std::to_string(expected_p + 1) + ":"
			);

			font24.draw(
				8 + (1.0 + cos(al_get_time() * 0.5)) * factor / 2,
				34 + 70 * bb - off_y * smooth_mult,
				path
			);

			m_draw.x_btn_items.draw(
				574,
				3 + 70 * bb - off_y * smooth_mult
			);
			it.set_collision_now_for_delete_top_y(3 + 70 * bb - off_y + 171);

			m_draw.item_frame.draw(0, 70 * bb - off_y * smooth_mult);
		}

		m_disp.set_as_target();
	}
	


	m_disp.flip();
}