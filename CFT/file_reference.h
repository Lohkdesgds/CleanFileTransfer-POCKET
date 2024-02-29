#pragma once

#include <string>

class File_reference  {
public:
	enum class e_status { READY_TO_SEND, RECEIVING, SENDING, ENDED_TRANSFER, ERROR_TRANSFER };
private:
	std::string m_file_path; // should not be changed hmm but cannot be const because vector needs copy

	e_status m_status = e_status::READY_TO_SEND;
	double m_progress = 0.0; // 0..1

	// x will be always known (draw now is 574, width is 27. height is also known. this is top y)
	int m_collision_position_now_for_delete_top_y = 0;

	int64_t m_was_hovering_hist_point = 0; // used for scroll
	bool m_hover = false;

	bool is_this_ours = true; // if ours, we are sending. If not, this is receiving
public:
	File_reference(const std::string&);

	e_status get_status() const;
	void set_status(e_status);

	double get_progress() const;
	void set_progress(double);

	bool get_is_ours() const;
	void set_is_ours(const bool);

	bool is_hovering() const;
	void set_if_zero_hovering_prop(int64_t);
	int64_t get_hovering_prop() const;
	void set_hovering(bool);


	int get_collision_now_for_delete_top_y() const;
	void set_collision_now_for_delete_top_y(int);

	const std::string& get_path() const;
	std::string get_resumed_name() const;
	std::string cut_path_to_max(const size_t) const;

	bool operator==(const std::string&) const;
	bool operator!=(const std::string&) const;
};