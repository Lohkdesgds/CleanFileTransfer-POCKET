#include "file_reference.h"

File_reference::File_reference(const std::string& s) : m_file_path(s)
{}

File_reference::e_status File_reference::get_status() const
{
	return m_status;
}

void File_reference::set_status(File_reference::e_status s)
{
	m_status = s;
}

double File_reference::get_progress() const
{
	return m_progress;
}

void File_reference::set_progress(double p)
{
	m_progress = p;
}

bool File_reference::get_is_ours() const
{
	return is_this_ours;
}

void File_reference::set_is_ours(const bool b)
{
	is_this_ours = b;
}

bool File_reference::is_hovering() const
{	
	return m_hover;
}

void File_reference::set_if_zero_hovering_prop(int64_t def_prop)
{
	if (m_was_hovering_hist_point == 0) m_was_hovering_hist_point = def_prop;
}

int64_t File_reference::get_hovering_prop() const
{
	return m_was_hovering_hist_point;
}

void File_reference::set_hovering(bool b)
{
	m_hover = b;
	if (!b && m_was_hovering_hist_point > 0) {
		m_was_hovering_hist_point = 0;
	}
}

int File_reference::get_collision_now_for_delete_top_y() const
{
	return m_collision_position_now_for_delete_top_y;
}

void File_reference::set_collision_now_for_delete_top_y(int y)
{
	m_collision_position_now_for_delete_top_y = y;
}

const std::string& File_reference::get_path() const
{
	return m_file_path;
}

std::string File_reference::get_resumed_name() const
{
	size_t p = m_file_path.rfind('\\');
	if (p == std::string::npos) p = m_file_path.rfind('/');
	if (p == std::string::npos) return m_file_path;
	return p + 1 >= m_file_path.size() ? m_file_path : m_file_path.substr(p + 1);
}

std::string File_reference::cut_path_to_max(const size_t l) const
{
	return (m_file_path.size() > l) ? m_file_path.substr(m_file_path.size() - l) : m_file_path;
}

bool File_reference::operator==(const std::string& s) const
{
	return s == m_file_path;
}

bool File_reference::operator!=(const std::string& s) const
{
	return s != m_file_path;
}