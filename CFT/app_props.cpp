#include "app_props.h"

#include <allegro5/allegro_primitives.h>

ItemDisplay::ItemDisplay(const std::string& file_track, const AllegroCPP::Bitmap& body_src, const AllegroCPP::Font* font_20) :
	m_file_ref(std::make_shared<File_reference>(file_track)),
	m_self_ref_for_sub(new AllegroCPP::Bitmap(body_src, 438, 854, 27 * 6, item_icon_ctl_height))
{
	if (!font_20) throw std::invalid_argument("null arg");
	//if (!s_refs) s_refs = new static_refs(body_src);

	m_item_name = new ClickableText(font_20, 0, 6, 568, 29, { {e_mouse_states_on_objects::DEFAULT, e_actions_object::NONE} }, " " + file_track);

	m_action_btn = new ClickableBitmap(*m_self_ref_for_sub, 573, -1, 27, item_icon_ctl_height, {
		{e_mouse_states_on_objects::DEFAULT, 0, 0} // change on the fly [4 == trash, 3 == download, 2 == upload, 1 == verified, 0 == alert]
		//{e_mouse_states_on_objects::HOVER, 27 * 4, 0},
		//{e_mouse_states_on_objects::CLICK, 27 * 4, 0},
		//{e_mouse_states_on_objects::CLICK_END, 27 * 4, 0},
	}, { 
		{e_mouse_states_on_objects::DEFAULT, e_actions_object::NONE}/*,
		{e_mouse_states_on_objects::CLICK_END, e_actions_object::DELETE_SELF}*/
	});

	m_trash_btn = new ClickableBitmap(*m_self_ref_for_sub, 573, -1, 27, item_icon_ctl_height, {
		{e_mouse_states_on_objects::DEFAULT, 27 * 5, 0}
	}, { 
		{e_mouse_states_on_objects::DEFAULT, e_actions_object::NONE},
		{e_mouse_states_on_objects::CLICK, e_actions_object::DELETE_SELF}
	});

	m_overlay = new ClickableBitmap(body_src, 0, 0, 600, height_of_items, { 
		{e_mouse_states_on_objects::DEFAULT, 0, 888}
	}, {
		{e_mouse_states_on_objects::DEFAULT, e_actions_object::NONE}
	});
}

ItemDisplay::~ItemDisplay()
{
	delete m_item_name;
	delete m_action_btn;
	delete m_overlay;
	delete m_trash_btn;

	delete m_self_ref_for_sub;

	m_file_ref.reset();
}

void ItemDisplay::refresh_self()
{
	if (!m_file_ref) throw std::runtime_error("Could not be null! at ItemDisplay file_ref!");
	const auto curr_status = m_file_ref->get_status();
	if (curr_status == m_last_file_ref_status) return;
	m_last_file_ref_status = curr_status;

	auto* i = m_action_btn->get_resource_when_on(e_mouse_states_on_objects::DEFAULT);
	auto* to_del = *i;

	switch (m_last_file_ref_status) {
	case File_reference::e_status::READY_TO_SEND:
		*i = new AllegroCPP::Bitmap(*m_self_ref_for_sub, 0, 0, 27, item_icon_ctl_height);
		m_progress_color_bg = al_map_rgb(84, 84, 84);
		m_progress_color_fg = al_map_rgb(217, 217, 217);
		break;
	case File_reference::e_status::RECEIVING:
		*i = new AllegroCPP::Bitmap(*m_self_ref_for_sub, 27 * 4, 0, 27, item_icon_ctl_height);
		m_progress_color_bg = al_map_rgb(84, 35, 83);
		m_progress_color_fg = al_map_rgb(255, 71, 252);
		break;
	case File_reference::e_status::SENDING:
		*i = new AllegroCPP::Bitmap(*m_self_ref_for_sub, 27 * 3, 0, 27, item_icon_ctl_height);
		m_progress_color_bg = al_map_rgb(35, 73, 84);
		m_progress_color_fg = al_map_rgb(71, 215, 255);
		break;
	case File_reference::e_status::ENDED_TRANSFER:
		*i = new AllegroCPP::Bitmap(*m_self_ref_for_sub, 27 * 2, 0, 27, item_icon_ctl_height);
		m_progress_color_bg = al_map_rgb(34, 83, 35);
		m_progress_color_fg = al_map_rgb(71, 255, 74);
		break;
	case File_reference::e_status::ERROR_TRANSFER:
		*i = new AllegroCPP::Bitmap(*m_self_ref_for_sub, 27 * 1, 0, 27, item_icon_ctl_height);
		m_progress_color_bg = al_map_rgb(84, 67, 35);
		m_progress_color_fg = al_map_rgb(217, 165, 61);
		break;
	}
	(*i)->set_draw_property(AllegroCPP::bitmap_position_and_flags{ 573, -1, 0 });

	delete to_del;
}

std::shared_ptr<File_reference> ItemDisplay::get_file_ref() const
{
	return m_file_ref;
}

e_actions_object ItemDisplay::check(const int(&mouse_pos)[2], e_mouse_states_on_objects mouse_state)
{
	if (m_file_ref->get_status() == File_reference::e_status::RECEIVING || m_file_ref->get_status() == File_reference::e_status::SENDING)
	{
		static int fake_mouse_pos_out_of_bounds[2] = { -9999, -9999 };
		m_item_name->check(fake_mouse_pos_out_of_bounds, mouse_state);
		m_overlay->check(fake_mouse_pos_out_of_bounds, mouse_state);
		m_action_btn->check(fake_mouse_pos_out_of_bounds, mouse_state);
		m_trash_btn->check(fake_mouse_pos_out_of_bounds, mouse_state);
		return e_actions_object::NONE;
	}
	else {
		m_item_name->check(mouse_pos, mouse_state);
		m_overlay->check(mouse_pos, mouse_state);
		m_action_btn->check(mouse_pos, mouse_state);
		return m_trash_btn->check(mouse_pos, mouse_state);
	}
	//return e_actions_object::NONE;
}

void ItemDisplay::draw(const AllegroCPP::Transform& base_transform, const int(&limits)[2][2]) const
{
	const ALLEGRO_COLOR animated_alpha_colored = al_map_rgba(0, 0, 0, 40.0 * (cos(al_get_time() * 4) + 1.0));

	al_set_clipping_rectangle(limits[0][0], limits[0][1], limits[1][0], limits[1][1]);

	const bool sel = m_overlay->get_last_mouse_state_checked() != e_mouse_states_on_objects::DEFAULT;

	// darken back of item on hover
	if (sel) al_draw_filled_rectangle(0, 0, 600, height_of_items, al_map_rgba(49, 49, 49, 50));


	// draw progress bar
	{
		al_draw_filled_rectangle(
			2, 3,
			574, 7,
			m_progress_color_bg
		);

		const float progress = static_cast<float>(m_file_ref->get_progress()) * 572;

		al_draw_filled_rectangle(
			2, 3,
			2 + progress, 7,
			m_progress_color_fg
		);

		// also animate glow
		al_draw_filled_rectangle(2, 3, 2 + progress, 7, animated_alpha_colored);
	}

	// draw name and action
	m_item_name->draw(base_transform);
	m_action_btn->draw();
	
	// animate action with glow
	al_draw_filled_rectangle(576, height_of_items - 7, 598, height_of_items - 3, animated_alpha_colored);


	m_overlay->draw();

	// show trash only if selected, clicked etc
	if (sel) m_trash_btn->draw();


	al_set_clipping_rectangle(0, 0, al_get_bitmap_width(al_get_target_bitmap()), al_get_bitmap_height(al_get_target_bitmap()));
} 




//ItemDisplay::static_refs* ItemDisplay::s_refs = nullptr;
//
//
//ItemDisplay::static_refs::static_refs(const AllegroCPP::Bitmap& body_src)
//{
//	// order: DEL, DOWNLOAD, UPLOAD, ENDED_GOOD, FAILED_ALERT // 573 546 519 492 465
//	s_buttons_mapping[0] = AllegroCPP::Bitmap(body_src, 573, 854, 27, 27);
//	s_buttons_mapping[1] = AllegroCPP::Bitmap(body_src, 546, 854, 27, 27);
//	s_buttons_mapping[2] = AllegroCPP::Bitmap(body_src, 519, 854, 27, 27);
//	s_buttons_mapping[3] = AllegroCPP::Bitmap(body_src, 492, 854, 27, 27);
//	s_buttons_mapping[4] = AllegroCPP::Bitmap(body_src, 465, 854, 27, 27);
//}