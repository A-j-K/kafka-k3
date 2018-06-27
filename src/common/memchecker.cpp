
#include <unistd.h>
#include <sys/epoll.h>

#include <ios>
#include <string>
#include <cstdio>
#include <csignal>
#include <fstream>
#include <sstream>
#include <iostream>

#include "memchecker.hpp"

using namespace std;

namespace K3
{

MemChecker::MemChecker() :
	_epoll_fd(0),
	_listener_fd(0),
	_system_limit(0),
	_program_limit(0),
	_overflowed(false),
	_mem_percent(.85),
	_used_filename(NULL),
	_limit_filename(NULL),
	_cgroup_memory_path(grep_cgroup_grep("memory"))	
{
	init();
}

MemChecker::MemChecker(double mem_percent) :
	_epoll_fd(0),
	_listener_fd(0),
	_system_limit(0),
	_program_limit(0),
	_overflowed(false),
	_mem_percent(mem_percent),
	_used_filename(NULL),
	_limit_filename(NULL),
	_cgroup_memory_path(grep_cgroup_grep("memory"))	
{
	init();
}

MemChecker::~MemChecker()
{
	if(_listener_fd > 0) {
		close(_listener_fd);
	}
	if(_epoll_fd) {
		close(_epoll_fd);
	}
	if(_limit_filename) free((void*)_limit_filename);
	if(_used_filename)  free((void*)_used_filename);
}

void
MemChecker::init() 
{
	if(_cgroup_memory_path.size() > 0) {
		stringstream filename;
		filename << _cgroup_memory_path << "/" << "memory.limit_in_bytes";
		_limit_filename = strdup(filename.str().c_str());
		ifstream memory_limit(_limit_filename);
		if(memory_limit.is_open()) {
			memory_limit >> _system_limit;
		}
	}
	if(_system_limit > 0) {
		stringstream filename;
		filename << _cgroup_memory_path << "/" << "memory.usage_in_bytes";
		_used_filename = strdup(filename.str().c_str());
		_program_limit = (long long)(((double)_system_limit) * _mem_percent);
#if 0
// Do we need to use epoll? Don't think so yet.
		_listener_fd = eventfd(0,0);
		_epoll_fd = epoll_create(1);
		if(_epoll_fd < 0) {
			close(_listener_fd);
			_system_limit = 0;
			_program_limit = 0;
		}
		else {
			struct epoll_event ev;
			ev.events = EPOLLIN;
			ev.data.ptr = (void*)this;
		}
#endif
	}
}

bool
MemChecker::ok()
{
	bool rval = true;
	if(_used_filename != NULL && _program_limit > 0) {
		ifstream used(_used_filename);
		if(used.is_open()) {
			long long current;
			used >> current;
			if(current > _program_limit) {
				rval = false;
			}
		}
		
	}
	return rval;
}

std::string
MemChecker::grep_cgroup_grep(const char *seek, const char *where)
{
	// Just like command line...
	//     grep cgroup <where> | grep <seek>
	// eg: grep cgroup /proc/mounts | grep memory
	if(!where) where = "/proc/mounts";
	string line, spath;
	ifstream proc_mounts(where, ios_base::in);
	while(proc_mounts.is_open() && getline(proc_mounts, line)) {
		size_t memory = line.find(seek);
		if(memory != string::npos) {
			size_t space = line.find_first_of(' ', 0);
			if(space != string::npos) {
				line = line.substr(space + 1);
				space = line.find_first_of(' ', 0);
				if(space != string::npos) {
					spath = line.substr(0, space);
					break;
				}
			}	
		}
	}

	return spath;
}


}; // end namespace K3

