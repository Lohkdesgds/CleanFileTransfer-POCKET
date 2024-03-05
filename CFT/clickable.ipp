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
inline void ClickableObject<Resource>::check_for_change_and_call_fcn(e_mouse_states_on_objects new_state_to_save)
{
	if (new_state_to_save == m_last_mouse_state) return;
	m_last_mouse_state = new_state_to_save;
	const auto it = m_fcn_map.find(m_last_mouse_state);
	if (it != m_fcn_map.end()) (it->second)(*this);
}

template<typename Resource>
inline c_state_resource_map_citerator<Resource> ClickableObject<Resource>::find_default() const
{
	if (const auto it = m_sub_rsc.find(e_mouse_states_on_objects::CUSTOM_1); 
		m_custom_1_bool_on && it != m_sub_rsc.end()) return it;
	return m_sub_rsc.find(e_mouse_states_on_objects::DEFAULT);
}

template<typename Resource>
inline const Resource& ClickableObject<Resource>::get_current_resource() const
{
	auto search = m_last_mouse_state;
	if (search == e_mouse_states_on_objects::DEFAULT) {
		if (m_custom_1_bool_on) 
			search = e_mouse_states_on_objects::CUSTOM_1;
	}

	auto it = m_sub_rsc.find(search);
	if (it == m_sub_rsc.end()) it = find_default();
	return it->second;
}

template<typename Resource>
inline ClickableObject<Resource>::ClickableObject(int draw_x, int draw_y, int w, int h, std::vector<ClickableObject<Resource>::cuts> cuts, c_state_action_map do_map, c_state_trig_fcn_map_auto fcn_map) :
	//m_sub_bmp(body_src, x, y, w, h),
	m_actions_map(do_map),
	m_fcn_map(fcn_map),
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
		check_for_change_and_call_fcn(e_mouse_states_on_objects::DEFAULT);
		return e_actions_object::NONE;
	}

	check_for_change_and_call_fcn(mouse_state);

	const auto it = m_actions_map.find(mouse_state);
	if (it == m_actions_map.end()) return e_actions_object::NONE;

	switch (it->second) {
	case e_actions_object::BOOLEAN_TOGGLE_DEFAULT_WITH_CUSTOM_1:
	//{
	//	auto a = m_sub_rsc.find(e_mouse_states_on_objects::DEFAULT);
	//	auto b = m_sub_rsc.find(e_mouse_states_on_objects::CUSTOM_1);
	//
	//	if (b == m_sub_rsc.end() || a == m_sub_rsc.end()) break;
	//
	//	std::swap(a->second, b->second);
	//}
		m_custom_1_bool_on = !m_custom_1_bool_on;
		break;
	default:
		break;
	}

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

template<typename Resource>
inline void ClickableObject<Resource>::set_function_on_state_changing_to(e_mouse_states_on_objects mouse_state, std::function<void(ClickableObject<Resource>&)> fcn)
{
	if (fcn) {
		m_fcn_map[mouse_state] = fcn;
	}
	else {
		auto it = m_fcn_map.find(mouse_state);
		if (it != m_fcn_map.end()) m_fcn_map.erase(it);
	}
}

template<typename Resource>
inline bool ClickableObject<Resource>::get_custom_1_state() const
{
	return m_custom_1_bool_on;
}

template<typename Resource>
inline void ClickableObject<Resource>::set_custom_1_state(bool b)
{
	m_custom_1_bool_on = b;
}
