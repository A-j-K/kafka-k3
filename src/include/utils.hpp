#pragma once

#include <map>
#include <memory>
#include <string>
#include <vector>
#include <sstream>

#include <librdkafka/rdkafkacpp.h>

namespace Utils {

bool
strcmp(const char *wild, const char *string);

bool
strcmp(const std::string &wild, const std::string &string);

typedef std::map<std::string, std::string> Metadata;

}; // namespace Utils

// http://stackoverview.blogspot.co.uk/2011/04/create-string-on-fly-just-in-one-line.html
struct stringbuilder
{
   std::stringstream ss;
   template<typename T>
   stringbuilder & operator << (const T &data)
   {
        ss << data;
        return *this;
   }
   operator std::string() { return ss.str(); }
};

