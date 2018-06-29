
#include <map>
#include <string>
#include <vector>
#include <memory>

#include <iostream>
#include <jansson.h>
#include <librdkafka/rdkafkacpp.h>

#include "s3.hpp"
#include "messagewrapper.hpp"

namespace K3 { 

class Produce 
{
protected:
	S3			*_ps3;
	std::ostream    	*_plog;
        RdKafka::Conf		*_pconf;

	const char 		**_ppenv;
	
	virtual void
	setup_general(json_t*);

	virtual void
	setup_default_global_conf(json_t*);

	virtual void
	setup_default_topic_conf(json_t*);

public:
	Produce();
	virtual ~Produce();

	virtual Produce&
	setLogStream(std::ostream *pstream) {
		_plog = pstream;
		return *this;
	}

	virtual Produce&
	setS3client(S3 *p) {
		_ps3 = p;
		return *this;
	}

	virtual S3*
	getS3client() {
		return _ps3;
	}

	virtual double
	getMemPercent() {
		return _mem_percent;
	}

	virtual Produce&
	setMemPercent(double d) {
		_mem_percent = d;
		return *this;
	}

	virtual void setup(json_t*, char **envp = NULL);

protected:

	virtual int64_t messageChecksum(const char *, size_t);

};

}; // namespace K3

