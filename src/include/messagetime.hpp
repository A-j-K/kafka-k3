#pragma once

#include <string>

namespace K3 {

// 2018-06-27-14-11-53.4120850916

class MessageTime
{
public:

	MessageTime();
	MessageTime(const std::string &);
	MessageTime(const MessageTime &other);
	virtual ~MessageTime();	

	bool operator == (MessageTime const& other);
	bool operator != (MessageTime const& other);
	bool operator >  (MessageTime const& other);
	bool operator >= (MessageTime const& other);
	bool operator <  (MessageTime const& other);
	bool operator <= (MessageTime const& other);

	virtual std::string to_string();

	virtual double timestamp() const;
	virtual void clear();

protected:

	int year;
	int month;
	int day;
	int hour;
	int minute;
	int second;
	double frac;

	std::string sfrac;

};

}; // end namespace K3



