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
	m_buf = text;
	for (auto& i : m_sub_rsc) {
		i.second->set_draw_property(AllegroCPP::font_position{ static_cast<float>(draw_x), static_cast<float>(draw_y) });
		//i.second->set_draw_property(text);
		if (w > 0 && h > 0) {
			i.second->set_draw_property(AllegroCPP::font_delimiter_justified{
				static_cast<float>(w), 0.0f, 0
			});
		}
	}
	apply_buf(); // easier
}

std::string& ClickableText::get_buf()
{
	return m_buf;
}

void ClickableText::apply_buf()
{
	const int width_max = m_click_zone[1][0] - m_click_zone[0][0];
	for (auto& i : m_sub_rsc) {
		if (width_max > 0) {
			std::string cpy = m_buf;
			while (1) {
				auto dim = i.second->get_dimensions(cpy);
				if (dim.w < width_max || cpy.length() == 0) break;
				cpy.erase(cpy.begin());
			}
			i.second->set_draw_property(cpy);
		}
		else {
			i.second->set_draw_property(m_buf);
		}
	}
}

void ClickableText::draw() const
{
	m_default_font->second->draw();
}