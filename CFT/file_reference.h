#pragma once

#include <string>

class File_reference {
public:
	enum class e_status { READY_TO_SEND, SENDING, ENDED_TRANSFER };
private:
	const std::string m_file_path;
	e_status m_status = e_status::READY_TO_SEND;
	double m_progress = 0.0;
public:
	File_reference(const std::string&);

	e_status get_status() const;
	void set_status(e_status);

	double get_progress() const;
	void set_progress(double);

	const std::string& get_path() const;
	std::string get_resumed_name() const;
	std::string cut_path_to_max(const size_t) const;
};