#include "app.h"

#include <allegro5/allegro_primitives.h>


void App::think_timed()
{
	if (static_cast<size_t>(m_smooth_scroll_target) + max_items_on_screen > m_item_list.size()) m_smooth_scroll_target = static_cast<double>(m_item_list.size() < max_items_on_screen ? 0 : m_item_list.size() - max_items_on_screen);
	if (m_smooth_scroll_target < 0.0) m_smooth_scroll_target = 0.0;

	m_smooth_scroll = (m_smooth_scroll * smoothness_scroll_y + m_smooth_scroll_target) / (1.0 + smoothness_scroll_y);

	if (fabs(m_smooth_scroll - m_smooth_scroll_target) < 0.01) m_smooth_scroll = m_smooth_scroll_target;

	m_d_drag_auto.auto_think(m_disp);
}

void App::hint_line_set(const std::string& str)
{
	if (!m_top_text) return;
	m_top_text->get_buf() = str;
	m_top_text->apply_buf();
}

void App::push_item_to_list(const std::string& path)
{
	std::lock_guard<std::recursive_mutex> l(m_item_list_mtx);
	for (const auto& i : m_item_list) { if (i->get_file_ref()->get_path() == path) return; }
	m_item_list.push_back(new ItemDisplay{ path, *m_base_png, m_font20 });
}

App::App() :
	m_disp(
		display_size[0], display_size[1], "CleanFileTransfer Ultimate", ALLEGRO_NOFRAME,
		AllegroCPP::display_undefined_position[0], AllegroCPP::display_undefined_position[1], 0,
		{ AllegroCPP::display_option{ ALLEGRO_VSYNC, 1, ALLEGRO_SUGGEST } }
	),
	m_font_resource(AllegroCPP::file_load_resource_name_to_temp_file(IDR_FONT1, "FONT_TYPE", ".ttf")),
	m_body_resource(AllegroCPP::file_load_resource_name_to_temp_file(IDR_PNG1, "PNG_TYPE", ".png")),
	m_base_png(new AllegroCPP::Bitmap(m_body_resource.clone_for_read(), 1024, 0, ".PNG")),
	m_font20(new AllegroCPP::Font(20, m_font_resource.clone_for_read())),
	m_font24(new AllegroCPP::Font(24, m_font_resource.clone_for_read())),
	m_font28(new AllegroCPP::Font(27, 28, m_font_resource.clone_for_read())),
	es_dnd(m_disp, EVENT_DROP_CUSTOM_ID),
	m_objects(_generate_all_items_in_screen())

{
	al_init_primitives_addon();

	// setup:
	m_disp.make_window_masked(DISPLAY_MASK_TRANSPARENT);
	es_draw_timer.set_speed(1.0 / (m_disp.get_refresh_rate() > 0 ? m_disp.get_refresh_rate() : default_monitor_refresh_rate));

	// queue preparation
	eq_draw << es_draw_timer;

	// think queue preparation
	eq_think << m_disp;
	eq_think << es_dnd;
	eq_think << AllegroCPP::Event_mouse();
	eq_think << AllegroCPP::Event_keyboard();
	eq_think << es_think_timed_stuff;

	// starts
	es_draw_timer.start();
	es_think_timed_stuff.start();

	//push_item_to_list("the_test_of_path.txt");
	for(int i = 0; i < 18; ++i) push_item_to_list("the_" + std::to_string(i) + "_test_of_path.txt");
}

App::~App()
{
	for (auto& i : m_objects) delete i;
	for (auto& i : m_item_list) delete i;
}

bool App::draw()
{
	static AllegroCPP::Transform tf_def;

	// default 1:1 transform
	tf_def.use();

	if (!eq_draw.wait_for_event().valid()) return !m_closed_flag;
	if (eq_draw.has_event()) eq_draw.flush_event_queue();

	m_disp.clear_to_color();
	for (const auto& i : m_objects) i->draw();

	AllegroCPP::Transform custom;
	const size_t offp = static_cast<size_t>(m_smooth_scroll);

	custom.translate(0, items_y_offset - m_smooth_scroll * height_of_items + offp * height_of_items);
	custom.use();

	{
		std::lock_guard<std::recursive_mutex> l(m_item_list_mtx);
		//for (const auto& i : m_item_list) {
		for (size_t p = offp; p < 18 + offp + (m_smooth_scroll != m_smooth_scroll_target ? 1 : 0) && p < m_item_list.size(); ++p) {
			auto& i = m_item_list[p];
			i->draw(custom, { {0, 171}, {600, 800 - 171} });
			custom.translate(0, height_of_items);
			custom.use();
		}
	}

	m_disp.flip();

	return !m_closed_flag;
}

bool App::think()
{
	auto ev = eq_think.wait_for_event(0.1f);
	if (!ev.valid()) return !m_closed_flag;

	const int _mouse_pos[2] = { ev.get().mouse.x, ev.get().mouse.y };

	std::vector<std::pair<decltype(m_item_list.begin()), e_actions_object>> m_actions_on_items;
	std::vector<std::pair<decltype(m_objects.begin()), e_actions_object>> m_actions_on_const_list;

	const auto mouse_check_auto = [&](e_mouse_states_on_objects state_test){
		for (auto i = m_objects.begin(); i != m_objects.end(); ++i) {
			auto ac = (*i)->check(_mouse_pos, state_test);
			if (ac != e_actions_object::NONE) m_actions_on_const_list.push_back({ i, ac });
		}

		int _mouse_fixed[2] = { _mouse_pos[0], _mouse_pos[1] };

		_mouse_fixed[1] -= items_y_offset - height_of_items * m_smooth_scroll;

		std::lock_guard<std::recursive_mutex> l(m_item_list_mtx);

		for (auto i = m_item_list.begin(); i != m_item_list.end(); ++i) {
			auto ac = (*i)->check(_mouse_fixed, state_test);
			if (ac != e_actions_object::NONE) m_actions_on_items.push_back({ i, ac });
			_mouse_fixed[1] -= height_of_items;
		}
	};

	const auto evr = ev.get();

	// Test stuff
	m_d_drag_auto.auto_event(evr);

	switch (evr.type) {
	case ALLEGRO_EVENT_MOUSE_AXES:
		mouse_check_auto(e_mouse_states_on_objects::HOVER);
		if (evr.mouse.dz != 0) {
			m_smooth_scroll_target -= evr.mouse.dz;
		}
		break;
	case ALLEGRO_EVENT_MOUSE_BUTTON_UP:
		mouse_check_auto(e_mouse_states_on_objects::CLICK_END);
		break;
	case ALLEGRO_EVENT_MOUSE_BUTTON_DOWN:
		mouse_check_auto(e_mouse_states_on_objects::CLICK);
		break;
	case ALLEGRO_EVENT_KEY_CHAR:
		if (!m_selected_target_for_text) break;

		switch (evr.keyboard.keycode) {
		case ALLEGRO_KEY_ESCAPE: case ALLEGRO_KEY_ENTER: case ALLEGRO_KEY_PAD_ENTER:
			m_selected_target_for_text = nullptr;
			break;
		case ALLEGRO_KEY_BACKSPACE:
			if (m_selected_target_for_text->get_buf().length() > 0) m_selected_target_for_text->get_buf().pop_back();
			m_selected_target_for_text->apply_buf();
			break;
		default:
			if (evr.keyboard.unichar > 0 && (isalnum(evr.keyboard.unichar) || evr.keyboard.unichar == ':' || evr.keyboard.unichar == '.') && m_selected_target_for_text->get_buf().length() < max_any_text_len)
				m_selected_target_for_text->get_buf() += evr.keyboard.unichar;
			m_selected_target_for_text->apply_buf();
			break;
		}
		break;
	case ALLEGRO_EVENT_TIMER:
		think_timed();
		break;
	case EVENT_DROP_CUSTOM_ID:
		{
			AllegroCPP::Drop_event ednd(evr);
			push_item_to_list(ednd.c_str());
		}
		break;
	}


	// Work on stuff
	for (const auto& i : m_actions_on_const_list) {
		switch (i.second) {
		case e_actions_object::DELETE_SELF:
			throw std::runtime_error("Const list of items shouldn't be able to call for delete! Cannot do that!");
			break;
		case e_actions_object::CLOSE_APP:
			m_closed_flag = true;
			break;
		case e_actions_object::UNSELECT_WRITE:
			m_selected_target_for_text = nullptr;
			break;
		case e_actions_object::SELECT_FOR_WRITE:
			if (!m_ipaddr_locked) m_selected_target_for_text = (ClickableText*)(*i.first);
			break;
		}
	}

	for (const auto& i : m_actions_on_items) {
		switch (i.second) {
		case e_actions_object::DELETE_SELF:
		{
			std::lock_guard<std::recursive_mutex> l(m_item_list_mtx);
			m_item_list.erase(i.first);
		}
			break;
		case e_actions_object::CLOSE_APP:
			m_closed_flag = true;
			break;
		}
	}


	return !m_closed_flag;
}

std::vector<ClickableBase*> App::_generate_all_items_in_screen()
{
	AllegroCPP::Bitmap*& bmp = m_base_png;
	AllegroCPP::Font*& f20 = m_font20;
	AllegroCPP::Font*& f24 = m_font24;
	AllegroCPP::Font*& f28 = m_font28;

	using bc = ClickableBitmap::bitmap_cuts;
	using ms = e_mouse_states_on_objects;
	using ac = e_actions_object;

	std::vector<ClickableBase*> objs;

	// Used later in references
	ClickableText* _text_host_client_switch = nullptr;

	/* Body */
	objs.push_back(new ClickableBitmap(
		bmp->make_ref(), 0, 0, display_size[0], display_size[1],
		{ bc{ms::DEFAULT, 0, 0} },
		{ 
			{ms::DEFAULT, ac::NONE},
			{ms::CLICK, ac::UNSELECT_WRITE} // always reset
		}
	));



	/* Title */
	objs.push_back(new ClickableText(
		f28, 8, -2, -1, -1,
		{ {ms::DEFAULT, ac::NONE} },
		"CFT Ultimate"
	));
	objs.push_back(m_top_text = new ClickableText(
		f20, 185, 3, 383, -1,
		{ {ms::DEFAULT, ac::NONE} },
		"Loading..."
	));

	/* Static texts ... */
	objs.push_back(new ClickableText(
		f28, 16, 42, -1, -1,
		{ {ms::DEFAULT, ac::NONE} },
		"Transfer role:"
	));
	objs.push_back(_text_host_client_switch = new ClickableText(
		f28, 208, 42, -1, -1,
		{ {ms::DEFAULT, ac::NONE} },
		" HOST " // changes to client later if clicked hmm
	));
	objs.push_back(new ClickableText(
		f28, 335, 42, -1, -1,
		{ {ms::DEFAULT, ac::NONE} },
		"Send/receive on?"
	));
	objs.push_back(new ClickableText(
		f28, 16, 90, -1, -1,
		{ {ms::DEFAULT, ac::NONE} },
		"IP:"
	));
	objs.push_back(new ClickableText(
		f24, 8, 139, -1, -1,
		{ {ms::DEFAULT, ac::NONE} },
		"Items to send / being downloaded (drag and drop!)"
	));
	/* ... static texts*/

	/* variable texts */
	objs.push_back(new ClickableText(
		f24, 64, 92, 528, 30, // 588, 92 if bigger than 529
		{ 
			{ms::DEFAULT, ac::NONE},
			{ms::CLICK_END, ac::SELECT_FOR_WRITE} // select if click ends here
		},
		"localhost"
	));
	/* ...variable texts */

	/* variable overlays */
	objs.push_back(new ClickableBitmap(
		bmp->make_ref(), 187, 40, 133, 38,
		{ bc{ms::DEFAULT, 0, 800}, bc{ms::CUSTOM_1, 0, 838} },
		{ {ms::DEFAULT, ac::NONE}, {ms::CLICK_END, ac::BOOLEAN_TOGGLE_DEFAULT_WITH_CUSTOM_1} },
		{	{ms::CLICK_END, [&, _text_host_client_switch](auto& self) {
				if (m_closed_flag) {
					self.set_custom_1_state(m_is_host);
					return;
				}
				m_is_host = self.get_custom_1_state();
				if (m_is_host) { _text_host_client_switch->get_buf() = " HOST "; hint_line_set("Became host. Hosting from now on!");}
				else		   { _text_host_client_switch->get_buf() = "CLIENT"; hint_line_set("Became client. Trying to connect asynchronously.");}
				_text_host_client_switch->apply_buf();
			}}
		}
	));
	objs.push_back(new ClickableBitmap(
		bmp->make_ref(), 555, 43, 32, 32,
		{ bc{ms::DEFAULT, 134, 800}, bc{ms::CUSTOM_1, 134, 832} },
		{ {ms::DEFAULT, ac::NONE}, {ms::CLICK_END, ac::BOOLEAN_TOGGLE_DEFAULT_WITH_CUSTOM_1} },
		{	{ms::CLICK_END, [&](auto& self) {
				m_is_send_receive_enabled = self.get_custom_1_state();
				if (m_is_send_receive_enabled) hint_line_set("Enabled send/receive! Tasking asynchronously.");
				else						   hint_line_set("Not receiving or sending new things anymore. Closing remaining connections, if any.");
			}}
		}
	));
	/* ...variable overlays */


	/* Exit button (must be last for security reasons) */
	objs.push_back(new ClickableBitmap(
		bmp->make_ref(), 572, 2, 27, 27,
		{ bc{ms::DEFAULT, 573, 800}, bc{ms::HOVER, 573, 827}, bc{ms::CLICK, 573, 827}, bc{ms::CLICK_END, 573, 827} },
		{ {ms::DEFAULT, ac::NONE}, {ms::CLICK_END, ac::CLOSE_APP} }
	));

	hint_line_set("Ready!");

	return objs;
}


void App::window_drag_wrk::auto_event(const ALLEGRO_EVENT& ev)
{
	static const auto check_in = [](const int(&p)[2], const int(&l)[2][2]) {
		return
			p[0] >= l[0][0] && p[0] < l[1][0] &&
			p[1] >= l[0][1] && p[1] < l[1][1];
	};

	switch (ev.type) {
	case ALLEGRO_EVENT_MOUSE_BUTTON_DOWN:
		if (check_in({ev.mouse.x, ev.mouse.y}, grab_window_limits)) {
			holding_window = true;
			offset_screen[0] = AllegroCPP::Mouse_cursor::get_pos_x();
			offset_screen[1] = AllegroCPP::Mouse_cursor::get_pos_y();
		}
		break;
	case ALLEGRO_EVENT_MOUSE_BUTTON_UP:
		holding_window = false;
		break;
	}
}

void App::window_drag_wrk::auto_think(AllegroCPP::Display& d) {
	if (holding_window) {
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
}


//const ALLEGRO_COLOR FrontEnd::mask_body = al_map_rgb(255, 0, 255);
//
//FrontEnd::display_work::display_work() :
//	m_font_resource(AllegroCPP::file_load_resource_name_to_temp_file(IDR_FONT1, "FONT_TYPE", ".ttf")),
//	m_body_png_resource(AllegroCPP::file_load_resource_name_to_temp_file(IDR_PNG1, "PNG_TYPE", ".png"))
//{
//	font_20 = new AllegroCPP::Font(20, m_font_resource.clone_for_read());
//	font_24 = new AllegroCPP::Font(24, m_font_resource.clone_for_read());
//	font_28 = new AllegroCPP::Font(27, 28, m_font_resource.clone_for_read());
//	body = new AllegroCPP::Bitmap(m_body_png_resource.clone_for_read(), 1024, 0, ".PNG");
//}
//
//FrontEnd::display_work::~display_work()
//{
//	delete font_24;
//	delete font_28;
//	delete body;
//	font_28 = font_24 = nullptr;
//	body = nullptr;
//	std::remove(m_body_png_resource.get_filepath().c_str());
//	std::remove(m_font_resource.get_filepath().c_str());
//}
//
//void FrontEnd::delayed_mouse_axes::post(ALLEGRO_MOUSE_EVENT ev)
//{
//	//me = ev;
//	m[0] = ev.x;
//	m[1] = ev.y;
//	dz_acc += ev.dz;
//	had = true;
//}
//
//bool FrontEnd::delayed_mouse_axes::consume()
//{
//	if (!had) return false;
//	had = false;
//	return true;
//}
//
//void FrontEnd::delayed_mouse_axes::reset()
//{
//	dz_acc = 0;
//}
//
//void FrontEnd::drag_functionality::move_what_is_possible(AllegroCPP::Display& d)
//{
//	const int to_move_x = AllegroCPP::Mouse_cursor::get_pos_x() - offset_screen[0];
//	const int to_move_y = AllegroCPP::Mouse_cursor::get_pos_y() - offset_screen[1];
//
//	int curr_x = 0, curr_y = 0;
//	d.get_position(curr_x, curr_y);
//
//	curr_x += to_move_x;
//	curr_y += to_move_y;
//	offset_screen[0] += to_move_x;
//	offset_screen[1] += to_move_y;
//
//	d.set_position(curr_x, curr_y);
//
//}
//
//
//FrontEnd::FrontEnd() : 
//	m_disp(display_size[0], display_size[1], "CleanFileTransfer Ultimate", ALLEGRO_NOFRAME),
//	m_evq(),
//	m_ev_dnd(m_disp, drag_and_drop_event_id),
//	m_data(),
//	m_draw()
//{
//	AllegroCPP::Monitor_info moninfo;
//	m_screen_time.set_speed(1.0 / (moninfo.m_refresh_rate <= 0 ? 60 : moninfo.m_refresh_rate));
//
//	m_screen_sync << m_screen_time;
//	m_screen_time.start();
//
//	m_evq << m_disp;
//	m_evq << m_ev_dnd;
//	m_evq << AllegroCPP::Event_mouse();
//	m_evq << AllegroCPP::Event_keyboard();
//	m_evq << m_smooth_anims;
//
//	m_draw.x_btn[0] = AllegroCPP::Bitmap(*m_draw.body, 573, 800, 27, 27);
//	m_draw.x_btn[1] = AllegroCPP::Bitmap(*m_draw.body, 573, 827, 27, 27);
//	m_draw.x_btn_items[0] = AllegroCPP::Bitmap(*m_draw.body, 573, 854, 27, 27);
//	m_draw.x_btn_items[1] = AllegroCPP::Bitmap(*m_draw.body, 546, 854, 27, 27);
//	m_draw.x_btn_items[2] = AllegroCPP::Bitmap(*m_draw.body, 519, 854, 27, 27);
//	m_draw.x_btn_items[3] = AllegroCPP::Bitmap(*m_draw.body, 492, 854, 27, 27);
//	m_draw.x_btn_items[4] = AllegroCPP::Bitmap(*m_draw.body, 465, 854, 27, 27);
//	m_draw.connect_btn[0] = AllegroCPP::Bitmap(*m_draw.body, 0, 845, 163, 43);
//	m_draw.connect_btn[1] = AllegroCPP::Bitmap(*m_draw.body, 0, 801, 163, 43);
//	m_draw.connect_btn[2] = AllegroCPP::Bitmap(*m_draw.body, 161, 801, 163, 43);
//	m_draw.slider_switch_host_client = AllegroCPP::Bitmap(*m_draw.body, 452, 800, 121, 35);
//	m_draw.item_frame = AllegroCPP::Bitmap(*m_draw.body, 0, 888, 600, item_shown_height);
//
//	m_draw.frame_of_items_mask_target = AllegroCPP::Bitmap(600, 629);
//
//	m_draw.font_24->set_draw_properties({
//		al_map_rgb(255,255,255),
//		AllegroCPP::font_multiline_b::MULTILINE
//	});	
//	m_draw.font_28->set_draw_properties({
//		al_map_rgb(255,255,255),
//		AllegroCPP::font_multiline_b::MULTILINE
//	});
//	//m_draw.body->set_draw_properties({
//	//	AllegroCPP::bitmap_scale{1.0f, 1.0f}
//	//});
//
//	m_draw.x_btn[0].set_draw_properties({ AllegroCPP::bitmap_position_and_flags{572, 2, 0} });
//	m_draw.x_btn[1].set_draw_properties({ AllegroCPP::bitmap_position_and_flags{572, 2, 0} });
//	m_draw.slider_switch_host_client.set_draw_properties({ AllegroCPP::bitmap_position_and_flags{189, 41, 0} }); // can do 116 to right
//	m_draw.connect_btn[0].set_draw_properties({ AllegroCPP::bitmap_position_and_flags{432, 37, 0} });
//	m_draw.connect_btn[1].set_draw_properties({ AllegroCPP::bitmap_position_and_flags{432, 37, 0} });
//	m_draw.connect_btn[2].set_draw_properties({ AllegroCPP::bitmap_position_and_flags{432, 37, 0} });
//
//	m_draw.frame_of_items_mask_target.set_draw_properties({ AllegroCPP::bitmap_position_and_flags{0, 171, 0} });
//
//	al_init_primitives_addon();
//
//	m_disp.make_window_masked(mask_body);
//	m_smooth_anims.start();
//}
//
//FrontEnd::~FrontEnd()
//{	
//}
//
//constexpr bool is_point_in(const int p[2], const int l[2][2])
//{
//	return
//		p[0] >= l[0][0] && p[0] < l[1][0] &&
//		p[1] >= l[0][1] && p[1] < l[1][1];
//}
//
//FrontEnd::e_event FrontEnd::task_events()
//{
//	//if (!m_evq.has_event()) return e_event::NONE;
//
//	auto ev = m_evq.wait_for_event();
//	if (!ev.valid()) return e_event::NONE;
//
//	const auto& evr = ev.get();
//
//
//	switch (evr.type) {
//	case ALLEGRO_EVENT_TIMER: // smooth animations!
//	{
//		if (m_draw.connect_btn_sel != e_connection_status::DISCONNECTED) // force block on connected status block
//		{
//			m_draw.kb_target = e_keyboard_input_targeting::NOTHING;
//		}
//
//		// main slider anim
//		if (m_draw.slider_switch_host_client_dir == 1) {
//			if (m_draw.slider_switch_host_client_pos_smooth < 306) {
//				m_draw.slider_switch_host_client_pos_smooth += (m_draw.slider_switch_host_client_pos_smooth < 286) ? 8 : 1;
//				m_draw.slider_switch_host_client.set_draw_property(AllegroCPP::bitmap_position_and_flags{ static_cast<float>(m_draw.slider_switch_host_client_pos_smooth), 41, 0 });
//			}
//		}
//		else {
//			if (m_draw.slider_switch_host_client_pos_smooth > 189) {
//				m_draw.slider_switch_host_client_pos_smooth -= (m_draw.slider_switch_host_client_pos_smooth > 209) ? 8 : 1;
//				m_draw.slider_switch_host_client.set_draw_property(AllegroCPP::bitmap_position_and_flags{ static_cast<float>(m_draw.slider_switch_host_client_pos_smooth), 41, 0 });
//			}
//		}
//		// item slider y axis move
//		{
//			const size_t factor = 1 +
//				(
//					m_draw.items_to_send_off_y < m_draw.items_to_send_off_y_target ?
//					m_draw.items_to_send_off_y_target - m_draw.items_to_send_off_y :
//					m_draw.items_to_send_off_y - m_draw.items_to_send_off_y_target
//				) / 4;
//
//			if (m_draw.items_to_send_off_y < m_draw.items_to_send_off_y_target)
//			{
//				m_draw.items_to_send_off_y += factor;
//			}
//			else if (m_draw.items_to_send_off_y > m_draw.items_to_send_off_y_target) 
//			{
//				m_draw.items_to_send_off_y -= factor;
//			}
//			else{
//				m_draw.items_to_send_temp_from_where_anim = static_cast<size_t>(-1); // no anim left
//			}
//		}
//		// always update bitmap once in a while for smooth stuff
//		m_draw.update_frame_of_items_mask_target = true;
//
//		// mouse axes delayed event for easier CPU usage
//		if (m_mouse_axes.consume())
//		{
//			if (m_ev_drag_window.holding_window) {
//				m_ev_drag_window.move_what_is_possible(m_disp);
//			}
//
//			const auto& mouse_axis = m_mouse_axes.m;
//			const auto& mouse_roll_positive_goes_up = m_mouse_axes.dz_acc;
//
//			if (m_draw.connect_btn_sel == e_connection_status::QUERY_DISCONNECT) m_draw.connect_btn_sel = e_connection_status::CONNECTED;
//
//			if (is_point_in(mouse_axis, connect_contrl_limits)) {
//				if (m_draw.connect_btn_sel == e_connection_status::CONNECTED) m_draw.connect_btn_sel = e_connection_status::QUERY_DISCONNECT;
//			}
//			else if (mouse_roll_positive_goes_up != 0 && is_point_in(mouse_axis, items_zone_limits)) {
//				m_draw.update_frame_of_items_mask_target = true;
//				m_draw.items_to_send_temp_from_where_anim = static_cast<size_t>(-1); // cancel anim
//
//				const auto real_idx = (m_draw.items_to_send_off_y_target / item_shown_height);
//
//				if (mouse_roll_positive_goes_up > 0 && real_idx > 0) m_draw.items_to_send_off_y_target -= item_shown_height;
//				if (mouse_roll_positive_goes_up < 0 && m_draw.items_to_send.size() - real_idx > max_items_shown_on_screen) m_draw.items_to_send_off_y_target += item_shown_height;
//			}
//
//			{ // in items list now
//				std::lock_guard<std::mutex> l(m_draw.items_to_send_mtx);
//
//				for (size_t bb = 0; bb < max_items_shown_on_screen + 1; ++bb)
//				{
//					const size_t expected_p = (m_draw.items_to_send_off_y / item_shown_height) + bb;
//					if (expected_p >= m_draw.items_to_send.size()) break;
//
//					auto& it = *m_draw.items_to_send[expected_p];
//
//					const int offy = it.get_collision_now_for_delete_top_y();
//					const int test[2][2] = { {0, offy}, {600, offy + static_cast<int>(item_shown_height)} };
//
//					it.set_hovering(is_point_in(mouse_axis, test));
//				}
//			}
//
//			m_mouse_axes.reset();
//		}
//	}
//		break;
//	case ALLEGRO_EVENT_DISPLAY_CLOSE:
//		return e_event::APP_CLOSE;
//	case ALLEGRO_EVENT_MOUSE_BUTTON_DOWN:
//	{
//		const int mouse_axis[2] = { evr.mouse.x, evr.mouse.y };
//
//		m_draw.x_btn_sel = 0;
//
//		if (is_point_in(mouse_axis, close_limits)) {
//			m_draw.x_btn_sel = 1;
//		}
//		else if (is_point_in(mouse_axis, grab_window_limits)) {
//			if (!m_ev_drag_window.holding_window) {
//				m_ev_drag_window.holding_window = true;
//				m_ev_drag_window.offset_screen[0] = AllegroCPP::Mouse_cursor::get_pos_x();
//				m_ev_drag_window.offset_screen[1] = AllegroCPP::Mouse_cursor::get_pos_y();
//				//printf_s("GET %d %d\n", m_ev_drag_window.offset_screen[0], m_ev_drag_window.offset_screen[1]);
//			}
//		}
//		else if (is_point_in(mouse_axis, switch_host_client_limits)) {
//			if (m_draw.connect_btn_sel == e_connection_status::DISCONNECTED) {
//				m_draw.slider_switch_host_client_dir = m_draw.slider_switch_host_client_dir < 0 ? 1 : -1;
//			}
//			else {
//				if (m_draw.slider_switch_host_client_dir < 0) {
//					m_draw.slider_switch_host_client_pos_smooth += 8;
//				}
//				else {
//					m_draw.slider_switch_host_client_pos_smooth -= 8;
//				}
//			}
//		}
//		else if (is_point_in(mouse_axis, connect_contrl_limits)) {
//			// VERY TEMP PART
//			//uint8_t tmp = static_cast<uint8_t>(m_draw.connect_btn_sel);
//			//tmp = (tmp + 1) % 5;
//			//m_draw.connect_btn_sel = static_cast<e_connection_status>(tmp);
//
//			return m_draw.connect_btn_sel == e_connection_status::DISCONNECTED ? e_event::WANT_CONNECT : e_event::WANT_DISCONNECT;
//		}
//		else { // in items list now
//			std::lock_guard<std::mutex> l(m_draw.items_to_send_mtx);
//
//			for (size_t bb = 0; bb < max_items_shown_on_screen + 1; ++bb)
//			{
//				const size_t expected_p = (m_draw.items_to_send_off_y / item_shown_height) + bb;
//				if (expected_p >= m_draw.items_to_send.size()) break;
//
//				auto& it = *m_draw.items_to_send[expected_p];
//
//				const int offy = it.get_collision_now_for_delete_top_y();
//				const int test[2][2] = { {574, offy}, {600, offy + 27} };
//
//				if (is_point_in(mouse_axis, test)) {
//					m_draw.items_to_send.erase(m_draw.items_to_send.begin() + expected_p);
//
//					if (m_draw.items_to_send_off_y_target >= item_shown_height) {
//						m_draw.items_to_send_off_y_target -= item_shown_height;
//						m_draw.items_to_send_temp_from_where_anim = expected_p;
//					}
//				}
//			}
//		}
//
//		// keyboard related
//		if (m_draw.connect_btn_sel != e_connection_status::DISCONNECTED) { // full block if connected
//			m_draw.kb_target = e_keyboard_input_targeting::NOTHING;
//		}
//		else if (is_point_in(mouse_axis, input_address_limits)) {
//			m_draw.kb_target = e_keyboard_input_targeting::IP_ADDR;
//		}
//		else {
//			m_draw.kb_target = e_keyboard_input_targeting::NOTHING;
//		}
//	}
//		break;
//	case ALLEGRO_EVENT_MOUSE_BUTTON_UP:
//	{
//		m_ev_drag_window.holding_window = false;
//
//		m_draw.x_btn_sel = 0;
//
//		const int mouse_axis[2] = { evr.mouse.x, evr.mouse.y };
//
//		if (is_point_in(mouse_axis, close_limits)) return e_event::APP_CLOSE;
//	}
//		break;
//	case ALLEGRO_EVENT_MOUSE_AXES:
//	{
//		// moved to timer for easier consume
//		m_mouse_axes.post(evr.mouse);
//	}
//		break;
//	case ALLEGRO_EVENT_KEY_CHAR:
//	{
//		const int key = evr.keyboard.unichar;
//
//		switch (evr.keyboard.keycode) {
//		case ALLEGRO_KEY_ESCAPE:
//		case ALLEGRO_KEY_ENTER:
//		case ALLEGRO_KEY_PAD_ENTER:
//			m_draw.kb_target = e_keyboard_input_targeting::NOTHING;
//			break;
//		case ALLEGRO_KEY_BACKSPACE:
//			if (m_draw.kb_target == e_keyboard_input_targeting::IP_ADDR) {
//				if (m_draw.ip_addr.length() > 0) m_draw.ip_addr.pop_back();
//			}
//			break;
//		default:
//			if (m_draw.kb_target == e_keyboard_input_targeting::IP_ADDR) {
//				if (key > 0 && (isalnum(key) || key == ':' || key == '.') && m_draw.ip_addr.length() < max_address_length) m_draw.ip_addr += key;
//			}
//			break;
//		}
//	}
//		break;
//	case drag_and_drop_event_id:
//	{
//		m_draw.update_frame_of_items_mask_target = true;
//		AllegroCPP::Drop_event ednd(evr);
//		{
//			std::lock_guard<std::mutex> l(m_draw.items_to_send_mtx);
//			auto it = std::find_if(m_draw.items_to_send.begin(), m_draw.items_to_send.end(), [&](const std::shared_ptr<File_reference> fr) {return *fr == ednd.c_str(); });
//			if (it == m_draw.items_to_send.end()) m_draw.items_to_send.push_back(std::make_shared<File_reference>(ednd.c_str()));
//		}
//	}
//		return e_event::POSTED_NEW_ITEMS;
//	}
//	return e_event::NONE;
//}
//
//void FrontEnd::task_draw()
//{
//	if (!m_screen_sync.wait_for_event().valid()) return; // wait for sync
//	if (m_screen_sync.has_event()) { // has in queue still?
//		m_screen_sync.flush_event_queue(); // skip
//	}
//
//	auto& font20 = *m_draw.font_20;
//	auto& font24 = *m_draw.font_24;
//	auto& font28 = *m_draw.font_28;
//	auto& body = *m_draw.body;
//
//	m_disp.clear_to_color();
//	body.draw();
//
//	m_draw.x_btn[m_draw.x_btn_sel].draw();
//	switch (m_draw.connect_btn_sel) {
//	case e_connection_status::DISCONNECTED:
//	case e_connection_status::CONNECTED:
//		m_draw.connect_btn[0].draw();
//		break;
//	case e_connection_status::CONNECTING:
//	case e_connection_status::DISCONNECTING:
//		m_draw.connect_btn[1].draw();
//		break;
//	case e_connection_status::QUERY_DISCONNECT:
//		m_draw.connect_btn[2].draw();
//		break;
//	}
//	m_draw.slider_switch_host_client.draw();
//
//	// top part
//
//	font28.draw(8, -2, "CleanFileTransfer Ultimate"); // orig: 8, 4. Got: 8, -2. Assume font -= 6
//
//	font28.draw(16, 42, "Transfer role:");
//	font24.draw(218, 44, "HOST");
//	font24.draw(324, 44, "CLIENT");
//
//	switch (m_draw.connect_btn_sel) {
//	case e_connection_status::DISCONNECTED:
//		font28.draw(477, 42, "Start!");
//		break;
//	case e_connection_status::CONNECTING:
//	case e_connection_status::DISCONNECTING:
//		font28.draw(452, 42, "Working...");
//		break;
//	case e_connection_status::CONNECTED:
//		font28.draw(447, 42, "Connected!");
//		break;
//	case e_connection_status::QUERY_DISCONNECT:
//		font28.draw(473, 42, "Close?");
//		break;
//	}
//
//	font28.draw(16, 90, "IP:");
//	{
//		auto str = m_draw.ip_addr + (m_draw.kb_target == e_keyboard_input_targeting::IP_ADDR && m_draw.ip_addr.length() < max_address_length ? (static_cast<uint64_t>(al_get_time() * 2) % 2 == 0 ? "_" : " ") : "");
//		const auto width = font24.get_width(str); // if > 529, x = 588
//		if (width >= 529) {
//			font24.set_draw_property(AllegroCPP::text_alignment::RIGHT);
//
//			while (font24.get_width(str) >= 529) str.erase(str.begin());
//
//			font24.draw(588, 92, str);
//			font24.set_draw_property(AllegroCPP::text_alignment::LEFT);
//		}
//		else {
//			font24.draw(64, 92, str);
//		}
//	}
//
//	// lower part
//
//	font24.draw(8, 139, "Items to send/receive (drag and drop, auto upload!)");
//	m_draw.frame_of_items_mask_target.draw();
//	
//	if (m_draw.update_frame_of_items_mask_target) {
//		m_draw.update_frame_of_items_mask_target = false;
//
//		m_draw.frame_of_items_mask_target.set_as_target();
//
//		// quick transform to alpha:
//		al_clear_to_color(al_map_rgb(0, 0, 0));
//		al_convert_mask_to_alpha(m_draw.frame_of_items_mask_target, al_map_rgb(0, 0, 0));
//				
//		std::lock_guard<std::mutex> l(m_draw.items_to_send_mtx);
//
//		for (size_t bb = 0; bb < max_items_shown_on_screen + 1; ++bb)
//		{
//			const size_t expected_p = (m_draw.items_to_send_off_y / item_shown_height) + bb;
//			const float off_y = m_draw.items_to_send_off_y % item_shown_height;
//
//			if (expected_p >= m_draw.items_to_send.size()) break;
//
//			auto& it = *m_draw.items_to_send[expected_p];
//			auto final_str = "#" + std::to_string(expected_p + 1) + ": " + it.get_path();
//			const auto width = font20.get_width(final_str);
//			const auto factor = width > max_width_text_item ? max_width_text_item - width : 0;
//			const float smooth_mult =  m_draw.items_to_send_temp_from_where_anim != static_cast<size_t>(-1) ? (expected_p < m_draw.items_to_send_temp_from_where_anim ? 1.0f : 0.0f) : 1.0f;
//			const auto off_y_calculated = item_shown_height * bb - off_y * smooth_mult;
//			const auto off_y_if_calculated_neg = off_y_calculated <= 0 ? off_y_calculated : 0;
//
//			auto masking = AllegroCPP::Bitmap(m_draw.frame_of_items_mask_target,
//				0, off_y_calculated <= 0 ? 0 : off_y_calculated,
//				600, item_shown_height + off_y_if_calculated_neg);
//
//			masking.set_as_target();
//
//			if (it.is_hovering()) {
//				al_draw_filled_rectangle(
//					1, 8 /*item_shown_height * bb - off_y * smooth_mult*/,
//					599, item_shown_height /*+ item_shown_height * bb - off_y * smooth_mult*/,
//					al_map_rgb(80, 135, 161)
//				);
//			}
//
//			{
//				int off_x = 0;
//				if (it.is_hovering()) {
//					// factor is the max offset in x. it is negative or zero
//
//					//const int base_off_x = static_cast<int>(al_get_time() * 100 + (expected_p * 8)); // this increases indefinitely
//					//const int ratioed = -(factor - (item_sliding_text_off_px_time * 2)); // calc limit. Positive now
//					//const int limited_base_x = base_off_x % ratioed; // this way we're limiting it. Positive
//					//const int offsetted_x = limited_base_x - item_sliding_text_off_px_time; // offset to negative
//
//					const int64_t time_off = static_cast<int64_t>(al_get_time() * 150 + (expected_p * 8));
//					it.set_if_zero_hovering_prop(time_off);
//
//					const int offsetted_x = (static_cast<int64_t>(time_off - it.get_hovering_prop()) % ((item_sliding_text_off_px_time * 2) - factor)) - item_sliding_text_off_px_time;
//
//
//					if (offsetted_x < 0) off_x = 0;
//					else if (offsetted_x > (-factor)) off_x = factor;
//					else off_x = -offsetted_x; // offset to left, that's why negative
//
//					//off_x = -offsetted_x;
//
//					//printf_s("%lli | %i\n", -off_x, factor);
//
//					//off_x = (item_sliding_text_off_px_time - (static_cast<int64_t>((al_get_time() * 10 + (expected_p * 8)) * factor) % (600 + (item_sliding_text_off_px_time * 2))));
//					// 
//					//if (off_x > 0) off_x = 0;
//					//if (off_x < factor) off_x = factor;
//				}
//				else if (width != 0) {
//					const auto base = "#" + std::to_string(expected_p + 1) + ": ...";
//					final_str = it.get_path();
//					
//					while (font20.get_width(base + final_str) > max_width_text_item && final_str.size()) final_str.erase(final_str.begin());
//					final_str = base + final_str;
//				}
//
//				font20.draw(
//					8 + off_x,
//					6 + off_y_if_calculated_neg,
//					final_str
//				);
//			}
//
//			//font20.draw(
//			//	8.0f + (1.0 + cos(al_get_time() * 0.5)) * static_cast<float>(factor) / 2.0f,
//			//	34 + item_shown_height * bb - off_y * smooth_mult,
//			//	path
//			//);
//
//
//			it.set_collision_now_for_delete_top_y(3 + item_shown_height * bb - off_y + 171);
//
//			{
//				// lower bar to show status even on hover
//
//				const int o = static_cast<int>(cos(al_get_time() * 5 + (expected_p * 3)) * 25);
//
//				switch (it.get_status()) {
//				case File_reference::e_status::READY_TO_SEND:
//					al_draw_filled_rectangle(575, 27, 599, 33, al_map_rgb(179 + o, 179 + o, 179 + o));
//					break;
//				case File_reference::e_status::RECEIVING:
//					al_draw_filled_rectangle(575, 27, 599, 33, al_map_rgb(217 + o, 61 + o, 215 + o));
//					break;
//				case File_reference::e_status::SENDING:
//					al_draw_filled_rectangle(575, 27, 599, 33, al_map_rgb(61 + o, 182 + o, 217 + o));
//					break;
//				case File_reference::e_status::ENDED_TRANSFER:
//					al_draw_filled_rectangle(575, 27, 599, 33, al_map_rgb(62 + o, 218 + o, 64 + o));
//					break;
//				case File_reference::e_status::ERROR_TRANSFER:
//					al_draw_filled_rectangle(575, 27, 599, 33, al_map_rgb(217 + o, 165 + o, 61 + o));
//					break;
//				}
//			}
//
//			// progress bar
//			if (7 + off_y_if_calculated_neg > 0.0f) { // can only draw bar if lower part of it is in screen!
//				float min_y = 3 + off_y_if_calculated_neg;
//				const float max_y = 7 + off_y_if_calculated_neg;
//
//				if (min_y <= 0.0f) min_y = 0.0f;
//
//				al_draw_filled_rectangle(
//					2, min_y,
//					574, max_y,
//					al_map_rgb(36,36,36)
//				);
//
//				const float progress_random = static_cast<float>((cos(al_get_time() + (expected_p * 0.1)) + 1.0) * 0.5) * 572;
//
//				al_draw_filled_rectangle(
//					2, min_y,
//					2 + progress_random, max_y,
//					al_map_rgb(210, 210, 210)
//				);
//			}
//
//			m_draw.item_frame.draw(0, off_y_if_calculated_neg);
//
//			if (it.is_hovering()) {
//				m_draw.x_btn_items[0].draw(573, -1 + off_y_if_calculated_neg);
//			}
//			else {
//				switch (it.get_status()) {
//				case File_reference::e_status::READY_TO_SEND:
//					m_draw.x_btn_items[0].draw(573, -1 + off_y_if_calculated_neg);
//					break;
//				case File_reference::e_status::RECEIVING:
//					m_draw.x_btn_items[1].draw(573, -1 + off_y_if_calculated_neg);
//					break;
//				case File_reference::e_status::SENDING:
//					m_draw.x_btn_items[2].draw(573, -1 + off_y_if_calculated_neg);
//					break;
//				case File_reference::e_status::ENDED_TRANSFER:
//					m_draw.x_btn_items[3].draw(573, -1 + off_y_if_calculated_neg);
//					break;
//				case File_reference::e_status::ERROR_TRANSFER:
//					m_draw.x_btn_items[4].draw(573, -1 + off_y_if_calculated_neg);
//					break;
//				}
//			}
//		}
//
//		m_disp.set_as_target();
//	}
//
//	m_disp.flip();
//}
//
//FrontEnd::e_connection_mode FrontEnd::get_connection_mode_selected() const
//{
//	return m_draw.slider_switch_host_client_dir > 0 ? e_connection_mode::CLIENT : e_connection_mode::HOST;
//}
//
//void FrontEnd::set_connection_status(e_connection_status e)
//{
//	m_draw.connect_btn_sel = e;
//}
//
//
//std::shared_ptr<File_reference> FrontEnd::get_next_ready_to_send_ref()
//{
//	for (auto i : m_draw.items_to_send) {
//		if (i->get_status() == File_reference::e_status::READY_TO_SEND) return i;
//	}
//	return {};
//}

