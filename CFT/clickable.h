#pragma once

#include <AllegroCPP/include/Graphics.h>
#include <AllegroCPP/include/System.h>

#include <memory>
#include <unordered_map>

#include "file_reference.h"

enum class e_actions_object { NONE, DELETE_SELF, CLOSE_APP, UNSELECT_ANY_TYPE, TYPE_IPADDR };
enum class e_mouse_states_on_objects { DEFAULT, HOVER, CLICK, CLICK_END };

using c_state_action_map = std::unordered_map<e_mouse_states_on_objects, e_actions_object>;
template<typename Resource> using c_state_resource_map = std::unordered_map<e_mouse_states_on_objects, Resource>;

class ClickableBase {
public:
	virtual ~ClickableBase() {}
	/// <summary>
	/// <para>Checks for mouse over it (selected)</para>
	/// <para>mouse_state is the event kind so the bitmap can know what to draw (hover, click, click_end only)</para>
	/// </summary>
	/// <param name="mouse_pos">Current mouse position</param>
	/// <param name="mouse_state">What event called this? Cannot be DEFAULT</param>
	/// <returns>NONE if none, or what was defined on creation.</returns>
	virtual e_actions_object check(const int(&mouse_pos)[2], e_mouse_states_on_objects mouse_state) { return e_actions_object::NONE; }

	/// <summary>
	/// <para>Based on check, it should know the correct way to draw (if you play the cards right)</para>
	/// </summary>
	virtual void draw() const {}
};

template<typename Resource>
class ClickableObject : public ClickableBase {
public:
	struct cuts { e_mouse_states_on_objects on_state; Resource use_this; };
protected:
	c_state_resource_map<Resource> m_sub_rsc;

	const c_state_action_map m_actions_map; // const map for mapping actions to resources
	const int m_click_zone[2][2]; // auto generate based on m_sub_rsc pos + subpart offset

	e_mouse_states_on_objects m_last_mouse_state = e_mouse_states_on_objects::DEFAULT;
public:
	ClickableObject(int draw_x, int draw_y, int w, int h, std::vector<cuts> cuts, c_state_action_map do_map);

	virtual e_actions_object check(const int(&mouse_pos)[2], e_mouse_states_on_objects mouse_state);

	/// <summary>
	/// <para>Get the resource used in this case (or null if no resource to this specific case)</para>
	/// </summary>
	/// <param name="state">Which state</param>
	/// <returns>Resource used in that state or null if none</returns>
	virtual Resource* get_resource_when_on(e_mouse_states_on_objects state);

	/// <summary>
	/// <para>Set a different resource in state case or add in not-yet-set case</para>
	/// </summary>
	/// <param name="state">Which state (any)</param>
	/// <param name="use_this">Resource to use from now on</param>
	virtual void set_resource_when_on(e_mouse_states_on_objects state, Resource use_this);

	/// <summary>
	/// <para>Removes this case on case -&gt; Resource map.</para>
	/// </summary>
	/// <param name="state">State being removed (cannot be DEFAULT)</param>
	/// <returns>True if there was a resource in there. False if not or error (invalid input)</returns>
	virtual bool remove_resource_on(e_mouse_states_on_objects state);

	/// <summary>
	/// <para>Just get back last mouse state registered at check.</para>
	/// </summary>
	/// <returns>Last tasked mouse state on this object</returns>
	virtual e_mouse_states_on_objects get_last_mouse_state_checked() const;

	/// <summary>
	/// <para>Check if action is part of this object behavior</para>
	/// </summary>
	/// <param name="act">Check for any possibility of this being returned from this object</param>
	/// <returns>True if this can cause this action</returns>
	virtual bool has_action_in_it(e_actions_object act) const;
};

#include "clickable.ipp"