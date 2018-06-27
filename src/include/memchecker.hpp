#pragma once

#include <sys/eventfd.h>

#include <string>

namespace K3
{

class MemChecker
{
public:
	MemChecker();
	MemChecker(double);
	virtual ~MemChecker();

	virtual std::string 
	get_cgroup_memory_path() { return _cgroup_memory_path; }

	virtual bool ok();

	virtual bool
	has_overflowed() { return _overflowed; }

protected:

	int		_epoll_fd;
	int		_listener_fd;
	long long	_system_limit;
	long long	_program_limit;

	bool		_overflowed;
	std::string	_cgroup_memory_path;

	const char *	_limit_filename;
	const char *	_used_filename;

	double		_mem_percent;

	virtual std::string
	grep_cgroup_grep(const char *seek = "memory", const char *where = NULL);

	virtual void init();

};

}; // end namespace K3

