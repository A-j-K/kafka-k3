
#include <memory>
#include <sstream>
#include <iomanip>
#include <iostream>

#include "messagetime.hpp" 


namespace K3 {

//           111111111122222222223
// 0123456789012345678901234567890
// 2018-06-27-14-11-53.4120850916

MessageTime::MessageTime():
	year(0),
	month(0),
	day(0),
	hour(0),
	minute(0),
	second(0),
	frac(0.)
{}

MessageTime::MessageTime(const std::string & in)
{
	std::string t;
	clear();
	t = in.substr(0, 4);
	if(t.size() > 0) year   = atoi(t.c_str());
	t = in.substr(5, 2);
	if(t.size() > 0) month  = atoi(t.c_str()); else month = 1;
	t = in.substr(8, 2);
	if(t.size() > 0) day    = atoi(t.c_str()); else day = 1;
	t = in.substr(11, 2);
	if(t.size() > 0) hour   = atoi(t.c_str());
	t = in.substr(14, 2);
	if(t.size() > 0) minute = atoi(t.c_str());
	t = in.substr(17, 2);
	if(t.size() > 0) second = atoi(t.c_str());
	t = in.substr(19);
	if(t.size() > 0) {
		frac   = atof(t.c_str());
		sfrac  = t;
	}
}

void
MessageTime::clear()
{
	year = 0;
	month = 0;
	day = 0;
	hour = 0;
	minute = 0;	
	second = 0;
	frac = 0.;
	sfrac = std::string();
}

MessageTime::MessageTime(const MessageTime &other)
{
	year   = other.year;
	month  = other.month;
	day    = other.day;
	hour   = other.hour;
	minute = other.minute;
	second = other.second;
	frac   = other.frac;
	sfrac  = other.sfrac;
}

MessageTime::~MessageTime()
{}

bool 
MessageTime::operator == (MessageTime const& other)
{
	if(year != other.year) return false;
	if(month != other.month) return false;
	if(day != other.day) return false;
	if(hour != other.hour) return false;
	if(minute != other.minute) return false;
	if(second != other.second) return false;
	if(frac != other.frac) return false;
	return true;
}

bool 
MessageTime::operator != (MessageTime const& other)
{
	return !(*this == other);
}

bool 
MessageTime::operator >  (MessageTime const& other)
{
	if(*this != other && timestamp() > other.timestamp()) return true;
	return false;
}

bool 
MessageTime::operator >= (MessageTime const& other)
{
	if(*this == other) return true;
	if(*this > other) return true;
	return false;
}

bool 
MessageTime::operator <  (MessageTime const& other)
{
	if(*this != other && timestamp() < other.timestamp()) return true;
	return false;
}

bool 
MessageTime::operator <= (MessageTime const& other)
{
	if(*this == other) return true;
	if(*this < other) return true;
	return false;
}

std::string 
MessageTime::to_string()
{
	std::stringstream oss;
	oss << std::setw(4) << year << "-";
	oss << std::setw(2) << std::setfill('0') << month << "-";
	oss << std::setw(2) << std::setfill('0') << day << "-";
	oss << std::setw(2) << std::setfill('0') << hour << "-";
	oss << std::setw(2) << std::setfill('0') << minute << "-";
	oss << std::setw(2) << std::setfill('0') << second ;
	oss << sfrac;
	return oss.str();
}

double
MessageTime::timestamp() const
{
	double rval = 0.;
	double leap = 0.;
	//                  Jan, Feb, Mar, Apr, May, Jun, Jul, Aug, Sep, Oct, Nov, Dec
	int months[] = { 0, 31,  28,  31,  30,  31,  30,  31,  31,  30,  31,  30,  31 }; 

	if(year % 4 == 0) {
		if(year % 100 == 0) {
			if(year % 400 == 0) {
				leap = (60*60*24);
			}
		}
	}

	rval += year * (60*60*24*365);
	rval += month * (60*60*24*months[month]);
	rval += day   * (60*60*24);
	rval += hour  * (60*60);
	rval += minute * (60);
	rval += second;
	rval += frac;
	rval += leap;
	return rval;
}

}; // end namespace K3



