#pragma once

#include "clickable.h"


class ClickableText : public ClickableObject<AllegroCPP::Font*> {
	//static constexpr double speed_relative_ = 0.016;

	const decltype(c_state_resource_map<AllegroCPP::Font*>{}.find(e_mouse_states_on_objects::DEFAULT)) m_default_font; // quick ref for only one font to keep format anyway

	std::string m_buf; // buffer between this and Font themselves
	std::string m_buf_set;
	//mutable double m_last_apply = 0.0;
public:
	ClickableText(const AllegroCPP::Font* font, int draw_x, int draw_y, int w, int h, c_state_action_map do_map, const std::string& text, c_state_trig_fcn_map_auto fcn_map = {});

	std::string& get_buf();
	void apply_buf();

	virtual void draw() const;
	virtual void draw(AllegroCPP::Transform base_transform) const;
};
