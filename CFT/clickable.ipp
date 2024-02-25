#pragma once
#include "clickable.h"

/* local-only functions */

// test col
constexpr static bool is_in(const int(&p)[2], const int(&l)[2][2])
{
	return
		p[0] >= l[0][0] && p[0] < l[1][0] &&
		p[1] >= l[0][1] && p[1] < l[1][1];
}


template<typename Resource>
inline ClickableObject<Resource>::ClickableObject(int draw_x, int draw_y, int w, int h, std::vector<ClickableObject<Resource>::cuts> cuts, c_state_action_map do_map) :
	//m_sub_bmp(body_src, x, y, w, h),
	m_actions_map(do_map),
	m_click_zone{ {draw_x, draw_y}, {draw_x + w, draw_y + h} }
{
	if (cuts.size() == 0)
		throw std::invalid_argument("There must be at least one cut on ClickableBitmap");

	if (std::find_if(cuts.begin(), cuts.end(),
		[](const ClickableObject<Resource>::cuts& cut) {
			return cut.on_state == e_mouse_states_on_objects::DEFAULT;
		}) == cuts.end()) throw std::invalid_argument("One of the states in cut map must be DEFAULT for default behavior on non-mapped states!");

		// All good, proceed

		for (const auto& cut : cuts) {
			m_sub_rsc.insert({ cut.on_state, cut.use_this/*AllegroCPP::Bitmap(body_src, cut.x, cut.y, w, h)*/ });
		}
}

template<typename Resource>
inline e_actions_object ClickableObject<Resource>::check(const int(&mouse_pos)[2], e_mouse_states_on_objects mouse_state)
{
	if (mouse_state == e_mouse_states_on_objects::DEFAULT) throw std::invalid_argument("mouse_state on ClickableBitmap cannot be DEFAULT.");

	if (!is_in(mouse_pos, m_click_zone)) {
		m_last_mouse_state = e_mouse_states_on_objects::DEFAULT;
		return e_actions_object::NONE;
	}

	m_last_mouse_state = mouse_state;

	const auto it = m_actions_map.find(mouse_state);
	if (it == m_actions_map.end()) return e_actions_object::NONE;
	return it->second;
}

template<typename Resource>
inline Resource* ClickableObject<Resource>::get_resource_when_on(e_mouse_states_on_objects state)
{
	const auto it = m_sub_rsc.find(state);
	if (it != m_sub_rsc.end()) return &it->second;
	return nullptr;
}

template<typename Resource>
inline void ClickableObject<Resource>::set_resource_when_on(e_mouse_states_on_objects state, Resource use_this)
{
	m_sub_rsc[state] = use_this;
}

template<typename Resource>
inline bool ClickableObject<Resource>::remove_resource_on(e_mouse_states_on_objects state)
{
	if (state == e_mouse_states_on_objects::DEFAULT) return false; // no no no!

	const auto it = m_sub_rsc.find(state);
	if (it != m_sub_rsc.end()) {
		m_sub_rsc.erase(it);
		return true;
	}

	return false;
}

template<typename Resource>
inline e_mouse_states_on_objects ClickableObject<Resource>::get_last_mouse_state_checked() const
{
	return m_last_mouse_state;
}

template<typename Resource>
inline bool ClickableObject<Resource>::has_action_in_it(e_actions_object act) const
{
	return std::find_if(
		m_actions_map.begin(),
		m_actions_map.end(),
		[&act](const std::pair<e_mouse_states_on_objects, e_actions_object>& i) {
			return i.second == act;
		}) != m_actions_map.end();
}
