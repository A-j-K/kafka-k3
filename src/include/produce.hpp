
#include <map>
#include <string>
#include <vector>
#include <memory>

#include <iostream>
#include <jansson.h>
#include <librdkafka/rdkafkacpp.h>

#include "checksum.hpp"
#include "messagewrapper.hpp"

namespace K3 { 

class Produce : public Checksum
{
protected:
	std::ostream    	*_plog;
        RdKafka::Conf		*_pconf;
	RdKafka::Topic 		*_ptopic;
	RdKafka::Producer 	*_pproducer;

	char 			**_ppenv = NULL;
	std::string		_target_topic;
	int32_t			_partition_dst;
	
public:
	Produce();
	Produce(char **envp);
	virtual ~Produce();

	virtual Produce&
	setLogStream(std::ostream *pstream) {
		_plog = pstream;
		return *this;
	}

	virtual int32_t
	get_dst_partition() { return _partition_dst; }

	virtual int
	produce(void *inp, size_t inlen, const void *inkey, size_t inkeylen, int32_t in_partition = RdKafka::Topic::PARTITION_UA);

	virtual void 
	setup(json_t *pjson = NULL, char **envp = NULL);

};

}; // namespace K3

