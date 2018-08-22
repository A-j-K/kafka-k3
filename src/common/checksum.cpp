
#include "checksum.hpp"

namespace K3 {

int64_t
Checksum::messageChecksum(const char *inp, size_t len) 
{
	int i = 0, j = 0;
	while(len--) {
		i += (int)((unsigned char)inp[j++]);
	}
	return i;
}

}; // namespace K3

