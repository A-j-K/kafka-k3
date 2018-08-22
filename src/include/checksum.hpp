#pragma once

#include <cstdlib>
#include <cstdint>

namespace K3 { 

class Checksum 
{
public:
	virtual int64_t messageChecksum(const char *, size_t);
};

}; // namespace K3

