
#include "utils.hpp"

namespace Utils {

bool
strcmp(const char *w, const char *s)
{
	const char *c = NULL, *m = NULL;
	if(w == NULL || s == NULL) return false;
	while ((*s) && (*w != '*')) {
		if ((*w != *s) && (*w != '?')) {
			return false;
		}
		w++;
		s++;
	}

	while (*s) {
		if (*w == '*') {
			if (!(*++w)) {
				return true;
			}
			m = w;
			c = s + 1;
		} 
		else if ((*w == *s) || (*w == '?')) {
			w++;
			s++;
		} 
		else {
			w = m;
			s = c++;
		}
	}

	while (*w == '*') {
		w++;
	}
	return (bool)!*w;
}

bool
strcmp(const std::string &wild, const std::string &string)
{
	return strcmp(wild.c_str(), string.c_str());
}

}; // namespace Utils

