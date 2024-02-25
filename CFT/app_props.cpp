#include "app_props.h"

#include <allegro5/allegro_primitives.h>

ItemDisplay::ItemDisplay(const std::string& file_track, const AllegroCPP::Bitmap& body_src, const AllegroCPP::Font* font_20) :
	m_file_ref(std::make_shared<File_reference>(file_track)),
	m_self_ref_for_sub(new AllegroCPP::Bitmap(body_src, 465, 854, 27 * 5, item_icon_ctl_height))
{
	if (!font_20) throw std::invalid_argument("null arg");
	//if (!s_refs) s_refs = new static_refs(body_src);

	m_item_name = new ClickableText(font_20, 8, 6, 568, 29, { {e_mouse_states_on_objects::DEFAULT, e_actions_object::NONE} }, file_track);

	m_action_btn = new ClickableBitmap(*m_self_ref_for_sub, 573, -1, 27, item_icon_ctl_height, {
		{e_mouse_states_on_objects::DEFAULT, 27 * (rand() % 4), 0} // change on the fly
		//{e_mouse_states_on_objects::HOVER, 27 * 4, 0},
		//{e_mouse_states_on_objects::CLICK, 27 * 4, 0},
		//{e_mouse_states_on_objects::CLICK_END, 27 * 4, 0},
	}, { 
		{e_mouse_states_on_objects::DEFAULT, e_actions_object::NONE}/*,
		{e_mouse_states_on_objects::CLICK_END, e_actions_object::DELETE_SELF}*/
	});

	m_trash_btn = new ClickableBitmap(*m_self_ref_for_sub, 573, -1, 27, item_icon_ctl_height, {
		{e_mouse_states_on_objects::DEFAULT, 27 * 4, 0}
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
		break;
	case File_reference::e_status::RECEIVING:
		*i = new AllegroCPP::Bitmap(*m_self_ref_for_sub, 27, 0, 27, item_icon_ctl_height);
		break;
	case File_reference::e_status::SENDING:
		*i = new AllegroCPP::Bitmap(*m_self_ref_for_sub, 54, 0, 27, item_icon_ctl_height);
		break;
	case File_reference::e_status::ENDED_TRANSFER:
		*i = new AllegroCPP::Bitmap(*m_self_ref_for_sub, 81, 0, 27, item_icon_ctl_height);
		break;
	case File_reference::e_status::ERROR_TRANSFER:
		*i = new AllegroCPP::Bitmap(*m_self_ref_for_sub, 108, 0, 27, item_icon_ctl_height);
		break;
	}

	delete to_del;
}

std::shared_ptr<File_reference> ItemDisplay::get_file_ref() const
{
	return m_file_ref;
}

e_actions_object ItemDisplay::check(const int(&mouse_pos)[2], e_mouse_states_on_objects mouse_state)
{
	m_item_name->check(mouse_pos, mouse_state);
	m_overlay->check(mouse_pos, mouse_state);
	m_action_btn->check(mouse_pos, mouse_state);
	return m_trash_btn->check(mouse_pos, mouse_state);	
	//return e_actions_object::NONE;
}

void ItemDisplay::draw() const
{
	const bool sel = m_overlay->get_last_mouse_state_checked() != e_mouse_states_on_objects::DEFAULT;

	// darken back of item on hover
	if (sel) al_draw_filled_rectangle(0, 0, 600, height_of_items, al_map_rgba(49, 49, 49, 50));

	m_item_name->draw();
	m_action_btn->draw();
	
	// animate action with glow
	al_draw_filled_rectangle(576, height_of_items - 7, 598, height_of_items - 3, al_map_rgba(0, 0, 0, 40.0 * (cos(al_get_time() * 4) + 1.0)));

	m_overlay->draw();

	// show trash only if selected, clicked etc
	if (sel) m_trash_btn->draw();
} 


std::vector<ClickableBase*> generate_all_items_in_screen(
	AllegroCPP::Bitmap* bmp,
	AllegroCPP::Font* f20,
	AllegroCPP::Font* f24,
	AllegroCPP::Font* f28
)
{
	using bc = ClickableBitmap::bitmap_cuts;
	using ms = e_mouse_states_on_objects;
	using ac = e_actions_object;

	std::vector<ClickableBase*> objs;

	/* Body */
	objs.push_back(new ClickableBitmap(
		bmp->make_ref(), 0, 0, display_size[0], display_size[1],
		{ bc{ms::DEFAULT, 0, 0} },
		{ 
			{ms::DEFAULT, ac::NONE},
			{ms::CLICK, ac::UNSELECT_ANY_TYPE} // always reset
		}
	));



	/* Title */
	objs.push_back(new ClickableText(
		f28, 8, -2, -1, -1,
		{ {ms::DEFAULT, ac::NONE} },
		"CleanFileTransfer Ultimate"
	));

	/* Static texts ... */
	objs.push_back(new ClickableText(
		f28, 16, 42, -1, -1,
		{ {ms::DEFAULT, ac::NONE} },
		"Transfer role:"
	));
	objs.push_back(new ClickableText(
		f28, 215, 42, -1, -1,
		{ {ms::DEFAULT, ac::NONE} },
		"HOST"
	));
	objs.push_back(new ClickableText(
		f28, 319, 42, -1, -1,
		{ {ms::DEFAULT, ac::NONE} },
		"CLIENT"
	));
	objs.push_back(new ClickableText(
		f28, 16, 90, -1, -1,
		{ {ms::DEFAULT, ac::NONE} },
		"IP:"
	));
	objs.push_back(new ClickableText(
		f24, 8, 139, -1, -1,
		{ {ms::DEFAULT, ac::NONE} },
		"Items to send/receive (drag and drop, auto upload!)"
	));
	/* ... static texts*/

	/* variable texts */
	objs.push_back(new ClickableText(
		f24, 64, 92, 528, 30, // 588, 92 if bigger than 529
		{ 
			{ms::DEFAULT, ac::NONE},
			{ms::CLICK_END, ac::TYPE_IPADDR} // select if click ends here
		},
		"localhost"
	));
	/* ...variable texts */


	/* Exit button (must be last for security reasons) */
	objs.push_back(new ClickableBitmap(
		bmp->make_ref(), 572, 2, 27, 27,
		{ bc{ms::DEFAULT, 573, 800}, bc{ms::HOVER, 573, 827}, bc{ms::CLICK, 573, 827}, bc{ms::CLICK_END, 573, 827} },
		{ {ms::DEFAULT, ac::NONE}, {ms::CLICK_END, ac::CLOSE_APP} }
	));

	return objs;
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