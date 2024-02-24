#pragma once

#include "clickable.h"


class ClickableText : public ClickableObject<AllegroCPP::Font*> {
	const decltype(c_state_resource_map<AllegroCPP::Font*>{}.find(e_mouse_states_on_objects::DEFAULT)) m_default_font; // quick ref for only one font to keep format anyway

	bool m_font_slideable_if_bigger_than_box = true;
public:
	ClickableText(const AllegroCPP::Font* font, int draw_x, int draw_y, int w, int h, c_state_action_map do_map, const std::string& text);

	virtual void draw() const;
};
