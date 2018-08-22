#pragma once

#include <ios>

namespace K3
{

// Ripped from https://www.boost.org/doc/libs/1_40_0/boost/io/ios_state.hpp

class IosGuard
{
public:
	typedef ::std::ios_base            state_type;
	typedef ::std::ios_base::fmtflags  aspect_type;

	explicit  IosGuard(state_type &s) : 
		s_save_(s), 
		a_save_(s.flags())
        {}

	IosGuard(state_type &s, aspect_type const &a) : 
		s_save_(s), 
		a_save_(s.flags(a))
        {}

	~IosGuard()
	{ this->restore(); }

	void  restore()
	{ s_save_.flags(a_save_); }

private:
	state_type &       s_save_;
	aspect_type const  a_save_;
	IosGuard& operator=(const IosGuard&);
};

}; // end namespace K3


