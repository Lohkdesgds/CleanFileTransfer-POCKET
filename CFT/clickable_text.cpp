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

ClickableText::ClickableText(const AllegroCPP::Font* font, int draw_x, int draw_y, int w, int h, c_state_action_map do_map, const std::string& text, c_state_triggered_functional_map fcn_map) :
	ClickableObject<AllegroCPP::Font*>(
		draw_x,
		draw_y,
		w,
		h,
		font_to_template_cut(font),
		do_map,
		fcn_map),
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
	//m_last_apply = al_get_time();
	//const int width_max = m_click_zone[1][0] - m_click_zone[0][0];

	for (auto& i : m_sub_rsc) {
		//if (width_max > 0) {
		//	std::string cpy = m_buf;
		//	//const int current_max_width = i.second->get_dimensions(cpy).w;
		//	//const int calculated_width_to_work_with = current_max_width - width_max;
		//	const int remove_front_every = (fmod(al_get_time(), 10.0) / 10.0) * 20;
		//
		//
		//	for (int c = 0;;) {
		//		auto dim = i.second->get_dimensions(cpy);
		//		if (dim.w < width_max || cpy.length() == 0) break;
		//
		//		if (++c < remove_front_every) {
		//			cpy.erase(cpy.begin());
		//		}
		//		else {
		//			cpy.pop_back();
		//			c = 0;
		//		}
		//	}
		//	i.second->set_draw_property(cpy);
		//}
		//else {
		i.second->set_draw_property(m_buf);
		//}
	}
	m_buf_set = m_buf;
}

void ClickableText::draw() const
{
	draw({});
}

void ClickableText::draw(AllegroCPP::Transform base_transform) const
{
	//if (al_get_time() - m_last_apply > time_seconds_between_auto_applies) apply_buf();
	const int width_max = m_click_zone[1][0] - m_click_zone[0][0];
	const auto targ_d = m_default_font->second->get_dimensions(m_buf_set);


	if (targ_d.w > width_max && width_max > 0) {
		const double diff = (targ_d.w - width_max);

		// 0..1
		const double translate_needed =
			std::max(0.0, std::min((fmod(al_get_time() * 60.0, diff + 140.0)) - 70.0, diff));

		AllegroCPP::Transform custom_transform = base_transform;

		float dx = 0.f, dy = 0.f; // off 1 + real val
		base_transform.transform_coordinates(dx, dy);

		int o_x, o_y, o_w, o_h;
		al_get_clipping_rectangle(&o_x, &o_y, &o_w, &o_h);

		const int ps[4] = {
			m_click_zone[0][0] + static_cast<int>(dx),
			m_click_zone[0][1] + static_cast<int>(dy),
			m_click_zone[1][0] - m_click_zone[0][0],
			m_default_font->second->get_line_height()
		};

		al_set_clipping_rectangle(
			ps[0] > o_x ? ps[0] : o_x,
			ps[1] > o_y ? ps[1] : o_y,
			ps[2] + ps[0] < o_x + o_w ? ps[2] : o_w,
			ps[3] + ps[1] < o_y + o_h ? ps[3] : o_h
		);

		custom_transform.translate(- translate_needed, 0);

		custom_transform.use();
		m_default_font->second->draw();
		base_transform.use();

		al_set_clipping_rectangle(o_x, o_y, o_w, o_h);
	}
	else m_default_font->second->draw();

}