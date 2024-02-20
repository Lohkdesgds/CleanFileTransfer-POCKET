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