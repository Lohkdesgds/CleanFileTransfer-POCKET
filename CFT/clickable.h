#pragma once

#include <AllegroCPP/include/Graphics.h>
#include <AllegroCPP/include/System.h>

#include <memory>
#include <unordered_map>

#include "file_reference.h"

enum class e_actions_object { 
	NONE, // does nothing
	CLOSE_APP, // close the app

	DELETE_SELF, // on a list, remove itself

	UNSELECT_WRITE, // unselects pointer, goes to null
	SELECT_FOR_WRITE, // select this as target for keyboard

	BOOLEAN_TOGGLE_DEFAULT_WITH_CUSTOM_1 // used with state CUSTOM_1 for toggle behavior. Toggles CUSTOM_1 with DEFAULT
};
enum class e_mouse_states_on_objects { 
	DEFAULT,
	HOVER,
	CLICK,
	CLICK_END,
	CUSTOM_1
};

using c_state_action_map = std::unordered_map<e_mouse_states_on_objects, e_actions_object>;
template<typename Resource> using c_state_resource_map = std::unordered_map<e_mouse_states_on_objects, Resource>;
using c_state_triggered_functional_map = std::unordered_map<e_mouse_states_on_objects, std::function<void(void)>>;

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
	c_state_triggered_functional_map m_fcn_map;

	const c_state_action_map m_actions_map; // const map for mapping actions to resources
	const int m_click_zone[2][2]; // auto generate based on m_sub_rsc pos + subpart offset

	e_mouse_states_on_objects m_last_mouse_state = e_mouse_states_on_objects::DEFAULT;

	virtual void check_for_change_and_call_fcn(e_mouse_states_on_objects new_state_to_save);
public:
	ClickableObject(int draw_x, int draw_y, int w, int h, std::vector<cuts> cuts, c_state_action_map do_map, c_state_triggered_functional_map fcn_map = {});

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

	/// <summary>
	/// <para>Set a function to be triggered at mouse state change on this object to this state</para>
	/// </summary>
	/// <param name="mouse_state">Changing to this state will trigger the function</param>
	/// <param name="fcn">Function to be triggered (or set to {} to remove it)</param>
	virtual void set_function_on_state_changing_to(e_mouse_states_on_objects mouse_state, std::function<void(void)> fcn = {});
};

#include "clickable.ipp"