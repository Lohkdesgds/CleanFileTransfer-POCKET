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

void FrontEnd::drag_functionality::apply_func_if_needed(AllegroCPP::Display& m_disp)
{
	if (is_dragging) {
		int x{}, y{};
		m_disp.get_position(x, y);
		x += mouse_dragging_offset[0];
		y += mouse_dragging_offset[1];
		m_disp.set_position(x, y);
		mouse_dragging_offset[0] = mouse_dragging_offset[1] = 0;
	}
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

	m_draw.x_btn[0] = AllegroCPP::Bitmap(*m_draw.body, 573, 800, 27, 27);
	m_draw.x_btn[1] = AllegroCPP::Bitmap(*m_draw.body, 573, 827, 27, 27);
	m_draw.connect_btn[0] = AllegroCPP::Bitmap(*m_draw.body, 0, 845, 163, 43);
	m_draw.connect_btn[1] = AllegroCPP::Bitmap(*m_draw.body, 0, 801, 163, 43);
	m_draw.connect_btn[2] = AllegroCPP::Bitmap(*m_draw.body, 161, 801, 163, 43);
	m_draw.slider_switch_host_client = AllegroCPP::Bitmap(*m_draw.body, 452, 800, 121, 35);
	m_draw.item_frame = AllegroCPP::Bitmap(*m_draw.body, 0, 888, 600, 70);

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

	m_disp.make_window_masked(mask_body);
	

	al_init_primitives_addon();
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
	// always tasked stuff
	{
		if (m_draw.slider_switch_host_client_dir == 1) {
			if (m_draw.slider_switch_host_client_pos_smooth < 306) {
				m_draw.slider_switch_host_client_pos_smooth++;
				m_draw.slider_switch_host_client.set_draw_property(AllegroCPP::bitmap_position_and_flags{ static_cast<float>(m_draw.slider_switch_host_client_pos_smooth), 4'1, 0 });
			}
		}
		else {
			if (m_draw.slider_switch_host_client_pos_smooth > 189) {
				m_draw.slider_switch_host_client_pos_smooth--;
				m_draw.slider_switch_host_client.set_draw_property(AllegroCPP::bitmap_position_and_flags{ static_cast<float>(m_draw.slider_switch_host_client_pos_smooth), 41, 0 });
			}
		}
	}

	if (!m_evq.has_event()) return e_event::NONE;

	auto ev = m_evq.get_next_event();
	if (!ev.valid()) return e_event::NONE;

	const auto& evr = ev.get();


	switch (evr.type) {
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
			m_ev_drag_window.is_dragging = true;
		}
		else if (is_point_in(mouse_axis, switch_host_client_limits)) {
			m_draw.slider_switch_host_client_dir = m_draw.slider_switch_host_client_dir < 0 ? 1 : -1;
		}
		else if (is_point_in(mouse_axis, connect_contrl_limits)) {
			uint8_t tmp = static_cast<uint8_t>(m_draw.connect_btn_sel);
			tmp = (tmp + 1) % 5;
			m_draw.connect_btn_sel = static_cast<e_connection_status>(tmp);
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
		m_ev_drag_window.apply_func_if_needed(m_disp);
		m_ev_drag_window.is_dragging = false;
		m_draw.x_btn_sel = 0;

		const int mouse_axis[2] = { evr.mouse.x, evr.mouse.y };

		if (is_point_in(mouse_axis, close_limits)) return e_event::APP_CLOSE;
	}
		break;
	case ALLEGRO_EVENT_MOUSE_AXES:
	{
		if (m_ev_drag_window.is_dragging) {
			m_ev_drag_window.mouse_dragging_offset[0] += evr.mouse.dx;
			m_ev_drag_window.mouse_dragging_offset[1] += evr.mouse.dy;
		}

		const int mouse_axis[2] = { evr.mouse.x, evr.mouse.y };

		if (m_draw.connect_btn_sel == e_connection_status::QUERY_DISCONNECT) m_draw.connect_btn_sel = e_connection_status::CONNECTED;

		if (is_point_in(mouse_axis, connect_contrl_limits)) {
			if (m_draw.connect_btn_sel == e_connection_status::CONNECTED) m_draw.connect_btn_sel = e_connection_status::QUERY_DISCONNECT;
		}
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

	
	for (size_t bb = 0; bb < 9; ++bb)
	{
		const size_t expected_p = m_draw.items_to_send_off_y + bb;

		if (expected_p < m_draw.items_to_send.size()) {
			auto& it = m_draw.items_to_send[expected_p];
			const auto& path = it.get_path();
			const auto width = font24.get_width(path);
			const auto factor = width > 590 ? 590 - width : 0;

			font24.draw(8, 175 + 70 * bb, "#" + std::to_string(expected_p + 1) + " | File in list:");
			font24.draw(8 + (1.0 + cos(al_get_time() * 0.5)) * factor / 2, 205 + 70 * bb, path); // was y = 170. as it is, 9 in screen at the same time
			m_draw.item_frame.draw(0, 171 + 70 * bb);
		}
		else break;
	}
	


	m_disp.flip();
}