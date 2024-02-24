#include "clickable_text.h"

#include <allegro5/allegro_primitives.h>

/* local-only functions */

// convert
static std::vector<ClickableObject<AllegroCPP::Font*>::cuts> font_to_template_cut(const AllegroCPP::Font* font)
{
	std::vector<ClickableObject<AllegroCPP::Font*>::cuts> m;
	m.push_back({ e_mouse_states_on_objects::DEFAULT, new AllegroCPP::Font(font->make_ref()) });
	return m;
}

ClickableText::ClickableText(const AllegroCPP::Font* font, int draw_x, int draw_y, int w, int h, c_state_action_map do_map, const std::string& text) :
	ClickableObject<AllegroCPP::Font*>(
		draw_x,
		draw_y,
		w,
		h,
		font_to_template_cut(font),
		do_map),
	m_default_font(m_sub_rsc.find(e_mouse_states_on_objects::DEFAULT))
{
	for (auto& i : m_sub_rsc) {
		i.second->set_draw_property(AllegroCPP::font_position{ static_cast<float>(draw_x), static_cast<float>(draw_y) });
		i.second->set_draw_property(text);
	}
}

void ClickableText::draw() const
{
	m_default_font->second->draw();
}