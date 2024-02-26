#pragma once

#include "clickable.h"

class ClickableBitmap : public ClickableObject<AllegroCPP::Bitmap*>  {
public:
	struct bitmap_cuts { e_mouse_states_on_objects on_state; int x, y; };

	ClickableBitmap(const AllegroCPP::Bitmap& body_src, int draw_x, int draw_y, int w, int h, std::vector<bitmap_cuts> cuts, c_state_action_map do_map, c_state_triggered_functional_map fcn_map = {});
	~ClickableBitmap();

	virtual void draw() const;
};

//constexpr std::vector<ClickableBitmap::bitmap_cuts> operator+(std::initializer_list<ClickableBitmap::bitmap_cuts> l) { return std::vector<ClickableBitmap::bitmap_cuts>{l.begin(), l.end()}; }
