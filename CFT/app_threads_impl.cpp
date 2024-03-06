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

void App::think_timed_slow()
{
	m_top_list_text->get_buf() = "To send / being downloaded (drag and drop!) [" + std::to_string(m_item_list.size()) + "]";
	m_top_list_text->apply_buf();

	for (auto& i : m_item_list) i->refresh_self();

	handle_socket_threads_make_them_on(m_is_send_receive_enabled);
}

bool App::_display_thread()
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

bool App::_think_thread()
{
	auto ev = eq_think.wait_for_event(0.1f);
	if (!ev.valid()) return !m_closed_flag;

	const int _mouse_pos[2] = { ev.get().mouse.x, ev.get().mouse.y };

	std::vector<std::pair<decltype(m_item_list.begin()), e_actions_object>> m_actions_on_items;
	std::vector<std::pair<decltype(m_objects.begin()), e_actions_object>> m_actions_on_const_list;

	const auto mouse_check_auto = [&](e_mouse_states_on_objects state_test) {
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
		{
			AllegroCPP::Keyboard kb;

			if (kb.get_key_down(ALLEGRO_KEY_LCTRL) || kb.get_key_down(ALLEGRO_KEY_RCTRL))
			{
				switch (evr.keyboard.keycode) {
				case ALLEGRO_KEY_V:
				{
					const auto expected = m_selected_target_for_text->get_buf() + m_disp.get_clipboard_text();
					m_selected_target_for_text->get_buf() = expected.substr(0, max_any_text_len);
					m_selected_target_for_text->apply_buf();
				}
					break;
				case ALLEGRO_KEY_C:
				{
					m_disp.set_clipboard_text(m_selected_target_for_text->get_buf());
				}
				break;
				case ALLEGRO_KEY_X:
				{
					m_disp.set_clipboard_text(m_selected_target_for_text->get_buf());
					m_selected_target_for_text->get_buf() = "";
					m_selected_target_for_text->apply_buf();
				}
					break;
				}
			}
			else {
				if (evr.keyboard.unichar > 0 && (isalnum(evr.keyboard.unichar) || evr.keyboard.unichar == ':' || evr.keyboard.unichar == '.') && m_selected_target_for_text->get_buf().length() < max_any_text_len)
					m_selected_target_for_text->get_buf() += evr.keyboard.unichar;
				m_selected_target_for_text->apply_buf();
			}
		}
			break;
		}
		break;
	case ALLEGRO_EVENT_TIMER:
		if (evr.timer.source == es_think_timed_stuff) think_timed();
		else										  think_timed_slow();
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

bool App::_socket_send_thread()
{
	try {
		if (m_closed_flag) return false;

		std::this_thread::sleep_for(std::chrono::milliseconds(100));

		if (!m_is_send_receive_enabled) {
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
			return !m_closed_flag;
		}

		if (!m_socket) return !m_closed_flag;

		std::shared_lock<std::shared_mutex> l_read(m_socket_mtx, std::defer_lock);
		if (!l_read.try_lock()) return !m_closed_flag;

		if (!m_socket) return !m_closed_flag;

		auto ptr = get_next_file_to_transfer();

		if (!ptr) {
			if (!ping_socket()) {
				hint_line_set("Ping failed? Cancelling...");
				m_is_send_receive_enabled = false;
			}
			else
				std::this_thread::sleep_for(std::chrono::milliseconds(100));
			return !m_closed_flag;
		}

		AllegroCPP::File fpr(ptr->get_resumed_name(), "rb");

		if (!fpr) {
			ptr->set_status(File_reference::e_status::ERROR_TRANSFER);
			return !m_closed_flag;
		}

		if (!send_file_incoming(ptr->get_resumed_name())) {
			ptr->set_status(File_reference::e_status::ERROR_TRANSFER);
			hint_line_set("FAILED TO SEND NAME, disconnected!");
			std::this_thread::sleep_for(std::chrono::milliseconds(500));
			m_is_send_receive_enabled = false;
			return !m_closed_flag;
		}

		m_socket_fp_send = std::unique_ptr<File_handler>(make_new_encrypter(*m_socket, fpr));
		ptr->set_status(File_reference::e_status::SENDING);
		ptr->set_progress(0.0);
		m_socket_fp_send->start_async();

		while (!m_socket_fp_send->has_ended()) {
			ptr->set_progress(m_socket_fp_send->get_progress() * 0.01);
			std::this_thread::sleep_for(std::chrono::milliseconds(50));
			if (!m_socket->valid()) {
				hint_line_set("FAILED IN THE MIDDLE OH NO");
				sockets_cleanup();
				ptr->set_status(File_reference::e_status::ERROR_TRANSFER);
				return !m_closed_flag;
			}

			if (m_closed_flag) {
				sockets_cleanup();
				ptr->set_status(File_reference::e_status::ERROR_TRANSFER);
			}
		}

		m_socket_fp_send.reset();

		ptr->set_status(File_reference::e_status::ENDED_TRANSFER);
		ptr->set_progress(1.0);
	}
	catch (const std::exception& e) {
		hint_line_set("EXCEPTION! " + std::string(e.what()));
		m_is_send_receive_enabled = false;
	}
	catch (...) {
		hint_line_set("EXCEPTION! Unknown?!");
		m_is_send_receive_enabled = false;
	}
	return !m_closed_flag;
}

bool App::_socket_recv_thread()
{
	try {
		if (m_closed_flag) return false;

		std::this_thread::sleep_for(std::chrono::milliseconds(50));

		// keep up to date
		m_selector_sockets->set_custom_1_state(!m_is_send_receive_enabled);

		if (!m_is_send_receive_enabled) {
			//{
			//	std::unique_lock<std::shared_mutex> l(m_socket_mtx, std::defer_lock);
			//	if (!l.try_lock()) return !m_closed_flag;
			//
			//	// it will check internally if there's a connection
			//	//goodbye_socket();
			//
			//	m_socket_if_host.reset();
			//	if (m_socket) {
			//		hint_line_set("Other side disconnected, reset!");
			//		m_is_send_receive_enabled = false;
			//		//sockets_cleanup();
			//	}
			//}

			std::this_thread::sleep_for(std::chrono::milliseconds(100));
			return !m_closed_flag;
		}

		if ((!m_socket || !m_socket->valid()) && m_is_host && !m_socket_if_host) { // if it is host and there's no host yet, create it
			std::unique_lock<std::shared_mutex> l(m_socket_mtx, std::defer_lock);
			if (!l.try_lock()) {
				hint_line_set("Cannot lock mutex for recv.");
				return !m_closed_flag;
			}

			m_socket_if_host = std::make_unique<AllegroCPP::File_host>(app_port);

			hint_line_set("Created host, waiting connection!");
			std::this_thread::sleep_for(std::chrono::milliseconds(500));

			return !m_closed_flag; // continue;
		}

		if (!m_socket || !m_socket->valid()) // no connection or not a valid one
		{
			hint_line_set("Checking to connect...");
			std::this_thread::sleep_for(std::chrono::milliseconds(250));

			std::unique_lock<std::shared_mutex> l(m_socket_mtx, std::defer_lock);
			if (!l.try_lock()) {
				hint_line_set("Cannot lock mutex for new connection.");
				return !m_closed_flag;
			}

			m_socket.reset();

			if (m_is_host) { // because of before check, m_socket_if_host must not be null here
				m_socket = std::make_unique<AllegroCPP::File_client>(m_socket_if_host->listen(500));
				if (m_socket->valid()) {
					const auto& expected_src_ip = m_ipaddr_src->get_buf();
					if (expected_src_ip.length() == 0 || (m_socket->valid() && m_socket->get_filepath() == expected_src_ip)) {
						m_socket_if_host.reset();

						hint_line_set("As host, all good! Handshake...");
						std::this_thread::sleep_for(std::chrono::milliseconds(100));

						if (!handshake_socket()) {
							m_socket.reset();
						}
						// good!
					}
					else {
						if (m_socket->valid() && m_socket->get_filepath().length()) hint_line_set("IP mismatch! Was '" + m_socket->get_filepath() + "'");
						else													    hint_line_set("Still waiting...");
						std::this_thread::sleep_for(std::chrono::milliseconds(250));

						m_socket.reset();
					}
				}
			}
			else { // if not host and not connected, try to connect.
				m_socket = std::make_unique<AllegroCPP::File_client>(m_ipaddr_src->get_buf(), app_port);
				if (!m_socket->valid()) {
					hint_line_set("Failed to connect. Trying again soon.");
					std::this_thread::sleep_for(std::chrono::milliseconds(200));

					m_socket.reset();
				}
				else { // seems good!
					hint_line_set("Connection seems good, sending HELLO...");
					std::this_thread::sleep_for(std::chrono::milliseconds(100));

					if (!handshake_socket()) {
						m_socket.reset();
						hint_line_set("HELLO failed.");
						std::this_thread::sleep_for(std::chrono::milliseconds(100));
					}
				}
			}
			return !m_closed_flag;
		}

		std::shared_lock<std::shared_mutex> l_shared(m_socket_mtx);

		b_socket_package_structure dat;
		if (m_socket->read(&dat, sizeof(dat)) != sizeof(b_socket_package_structure)) {
			hint_line_set("FATAL ERROR ON SOCKET! DEAD!");
			m_is_send_receive_enabled = false;

			//l_shared.release();
			//std::unique_lock<std::shared_mutex> l(m_socket_mtx, std::defer_lock);
			//if (!l.try_lock()) return !m_closed_flag;
			//
			//sockets_cleanup();
			return !m_closed_flag;
		}

		switch (static_cast<e_socket_package>(dat.socket_package)) {
		case e_socket_package::CLOSING_BYE:
		{
			hint_line_set("Other side disconnected, bye!");
			m_is_send_receive_enabled = false;
		}
		break;
		case e_socket_package::PING:
			hint_line_set("Connection is online!");
			break;
		case e_socket_package::FILE_DESCRIPTION_AND_BEGIN:
		{
			hint_line_set("Receiving file " + std::string(dat.data.raw));
			AllegroCPP::File fpw("received_" + std::string(dat.data.raw), "wb");

			ItemDisplay* receiving = new ItemDisplay("received_" + std::string(dat.data.raw), *m_base_png, m_font20);
			const auto ptr = receiving->get_file_ref();

			ptr->set_status(File_reference::e_status::RECEIVING);
			ptr->set_progress(0.0);

			m_item_list.push_back(receiving);

			m_socket_fp_recv = std::unique_ptr<File_handler>(make_new_decrypter(fpw, *m_socket));
			m_socket_fp_recv->start_async();

			while (!m_socket_fp_recv->has_ended()) {
				ptr->set_progress(m_socket_fp_recv->get_progress() * 0.01);

				if (!m_socket->valid()) {
					hint_line_set("FAILED IN THE MIDDLE OH NO");
					m_is_send_receive_enabled = false;
					//sockets_cleanup();
					ptr->set_status(File_reference::e_status::ERROR_TRANSFER);
					return !m_closed_flag;
				}

				if (m_closed_flag) {
					//sockets_cleanup();
					m_is_send_receive_enabled = false;
					ptr->set_status(File_reference::e_status::ERROR_TRANSFER);

					//hint_line_set("Can't close while sending a file. Please wait next time!");
					//AllegroCPP::message_box("Error", "Transfer failed, cannot close while in the middle of transfer", "App will abort completely.");
					//std::terminate();
				}
			}

			m_socket_fp_recv.reset();

			ptr->set_status(File_reference::e_status::ENDED_TRANSFER);
			ptr->set_progress(1.0);
		}
		break;
		}
	}
	catch (const std::exception& e) {
		hint_line_set("EXCEPTION! " + std::string(e.what()));
		m_is_send_receive_enabled = false;
	}
	catch (...) {
		hint_line_set("EXCEPTION! Unknown?!");
		m_is_send_receive_enabled = false;
	}
	return !m_closed_flag;
}