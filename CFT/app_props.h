#pragma once

#include <AllegroCPP/include/Graphics.h>
#include <AllegroCPP/include/System.h>

#include "file_reference.h"
#include "clickable_bitmap.h"
#include "clickable_text.h"
#include "resource.h"

// Constants:

constexpr int display_size[2] = { 600, 800 };
constexpr int default_monitor_refresh_rate = 240;
constexpr int height_of_items = 35;
constexpr int items_y_offset = 171;
constexpr int item_icon_ctl_height = 33;
constexpr size_t max_any_text_len = 512;

constexpr int EVENT_DROP_CUSTOM_ID = 1024;

#define DISPLAY_MASK_TRANSPARENT al_map_rgb(255, 0, 255)


// Structs & Classes

// All items are at Y = 0. We should use AllegroCPP::Transform to translate coordinates so this item in the list shows in the correct position
class ItemDisplay : public ClickableBase {
	// non copyable and movable
	ItemDisplay(const ItemDisplay&) = delete; ItemDisplay(ItemDisplay&&) = delete; void operator=(const ItemDisplay&) = delete; void operator=(ItemDisplay&&) = delete;

	enum class e_buttons_mapping {DEL, DOWNLOAD, UPLOAD, ENDED_GOOD, FAILED_ALERT};

	//struct static_refs {
	//	AllegroCPP::Bitmap s_buttons_mapping[5]; // delete, download, up, ended good, alert
	//
	//	static_refs(const AllegroCPP::Bitmap& body_src);
	//};
	//
	//static static_refs* s_refs;

	std::shared_ptr<File_reference> m_file_ref; // a file reference for progress and to keep action btn up to date
	File_reference::e_status m_last_file_ref_status = File_reference::e_status::READY_TO_SEND;

	AllegroCPP::Bitmap* m_self_ref_for_sub; // inverse order, 27 * 4 pixels to right = e_buttons_mapping::DEL ... FAILED_ALERT on 0

	ClickableText* m_item_name; // Like: #NUM: Path_name_scrollable_on_hover
	ClickableBitmap
		*m_overlay, // whole overlay png
		*m_action_btn, // selected icon, under trash
		*m_trash_btn; // over action icon

	// the rest are darken background etc

public:
	ItemDisplay(const std::string& file_track, const AllegroCPP::Bitmap& body_src, const AllegroCPP::Font* font_20);
	~ItemDisplay();

	void refresh_self();
	std::shared_ptr<File_reference> get_file_ref() const;
	
	e_actions_object check(const int(&mouse_pos)[2], e_mouse_states_on_objects mouse_state);
	void draw() const;
};


// Functions:

std::vector<ClickableBase*> generate_all_items_in_screen(AllegroCPP::Bitmap* bmp, AllegroCPP::Font* f20, AllegroCPP::Font* f24, AllegroCPP::Font* f28);