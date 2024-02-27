#include "clickable_bitmap.h"

#include <allegro5/allegro_primitives.h>

/* local-only functions */

// convert
static std::vector<ClickableObject<AllegroCPP::Bitmap*>::cuts> bitmap_cuts_to_template_cut(const AllegroCPP::Bitmap& body_src, int w, int h, const std::vector<ClickableBitmap::bitmap_cuts>& cuts)
{
	std::vector<ClickableObject<AllegroCPP::Bitmap*>::cuts> m;
	for (const auto& cut : cuts)
		m.push_back({ cut.on_state, new AllegroCPP::Bitmap(body_src, cut.x, cut.y, w, h) });
	return m;
}

ClickableBitmap::ClickableBitmap(const AllegroCPP::Bitmap& body_src, int draw_x, int draw_y, int w, int h, std::vector<bitmap_cuts> cuts, c_state_action_map do_map, c_state_trig_fcn_map_auto fcn_map) :
	ClickableObject<AllegroCPP::Bitmap*>(
		draw_x,
		draw_y,
		w,
		h,
		bitmap_cuts_to_template_cut(body_src, w, h, cuts),
		do_map,
		fcn_map)
{
	for (auto& i : m_sub_rsc) {
		i.second->set_draw_property(AllegroCPP::bitmap_position_and_flags{ static_cast<float>(draw_x), static_cast<float>(draw_y), 0 });
	}
}

ClickableBitmap::~ClickableBitmap()
{
	for (auto& i : m_sub_rsc) delete i.second;
	m_sub_rsc.clear();
}

void ClickableBitmap::draw() const
{
	//auto it = m_sub_rsc.find(m_last_mouse_state);
	//if (it == m_sub_rsc.end()) it = m_sub_rsc.find(e_mouse_states_on_objects::DEFAULT);
	//it->second->draw();
	(get_current_resource())->draw();
}