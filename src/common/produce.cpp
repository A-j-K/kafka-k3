
#include <map>
#include <string>
#include <vector>
#include <memory>
#include <algorithm>
#include <stdexcept>
#include <exception>

#include "utils.hpp"
#include "produce.hpp"

namespace K3 { 

Produce::Produce() :
	_ppenv(0),
	_pconf(0),
	_plog(&std::cout),
	_partition_dst(RdKafka::Topic::PARTITION_UA)
{}

Produce::Produce(char **envp) :
	_ppenv(envp),
	_pconf(0),
	_plog(&std::cout),
	_partition_dst(RdKafka::Topic::PARTITION_UA)
{}

Produce::~Produce()
{
	if(_ptopic) delete _ptopic;
	if(_pproducer) delete _pproducer;
}

int
Produce::produce(void *inp, size_t inlen, const void *inkey, size_t inkeylen, int32_t in_partition)
{
	int rval = 0;

	if(_partition_dst == RdKafka::Topic::PARTITION_UA || _partition_dst == in_partition) {
		RdKafka::ErrorCode resp = 
			_pproducer->produce(
				_ptopic, 
				in_partition, 
				RdKafka::Producer::RK_MSG_COPY,
				inp, 
				inlen, 
				inkey,
				inkeylen,
				NULL
			);
		if(resp != RdKafka::ERR_NO_ERROR) {
			rval = -1;
			*_plog << "Produce failed: " << RdKafka::err2str(resp) << std::endl;
		}
		else {
			while(_pproducer->outq_len() > 0) {
				_pproducer->poll(5);
			}
		}
	}

	return rval;
}

void 
Produce::setup(json_t *inp, char **envp)
{
	const char *s;
	std::string errstr;
	RdKafka::Conf *tconf = RdKafka::Conf::create(RdKafka::Conf::CONF_TOPIC);

	_pconf = RdKafka::Conf::create(RdKafka::Conf::CONF_GLOBAL);

	if((s = std::getenv("KAFKA_SECURITY_PROTOCOL")) != NULL) {
		std::string ss(s);
		*_plog << "Setting security protocol" << ss << std::endl;
		_pconf->set("security.protocol", ss, errstr);
		if(ss.find("ssl") != std::string::npos) {
			const char *pem = "/var/ca/ca.pem";
			if((s = std::getenv("KAFKA_CA_FILE")) != NULL) {
				pem = s;
			}
			*_plog << "Setting cipher.suites DHE-DSS-AES256-GCM-SHA384" << pem << std::endl;
			_pconf->set("ssl.cipher.suites", "DHE-DSS-AES256-GCM-SHA384", errstr);
			*_plog << "Setting ca location" << pem << std::endl;
			_pconf->set("ssl.ca.location", pem, errstr);
		}
		else {
			*_plog << "No SSL found" << std::endl;
		}
	}
	if((s = std::getenv("KAFKA_SASL_MECHANISMS")) != NULL) {
		_pconf->set("sasl.mechanisms", std::string(s), errstr);
	}
	if((s = std::getenv("KAFKA_USER")) != NULL) {
		_pconf->set("sasl.username", std::string(s), errstr);
	}
	if((s = std::getenv("KAFKA_PASS")) != NULL) {
		_pconf->set("sasl.password", std::string(s), errstr);
	}
	if((s = std::getenv("KAFKA_BROKERS")) != NULL) {
		_pconf->set("metadata.broker.list", std::string(s), errstr);
	}
	else {
		throw std::invalid_argument(
			stringbuilder()
			<< "No broker list ENV var supplied"
		);
	}
	if((s = std::getenv("KAFKA_TARGET_TOPIC")) != NULL) {
		_target_topic = std::string(s);
	}
	else {
		throw std::invalid_argument(
			stringbuilder()
			<< "No topic ENV var supplied"
		);
	}
	if((s = std::getenv("KAFKA_TOPIC_PARTITION_DESTINATION")) != NULL) {
		_partition_dst = atoi(s);
	}


	if(envp || _ppenv) {
		std::string comp("RDKAFKA_SETVAR_");
		for(char **env = envp != NULL ? envp : _ppenv; *env != 0; env++) {
			std::string fullenvname(*env);
			std::string subenvname = fullenvname.substr(0, comp.size());
			if(subenvname == comp) {
				std::string envname = fullenvname.substr(comp.size());
				std::string key = envname.substr(0, envname.find("="));
				std::string val = envname.substr(key.size()+1);
				std::replace(key.begin(), key.end(), '_', '.');
				std::transform(key.begin(), key.end(), key.begin(), ::tolower);
				if(key.size() > 0 && val.size() > 0) {
					_pconf->set(key, val, errstr);
				}
			}
                }
	}

	_pproducer = RdKafka::Producer::create(_pconf, errstr);
	if(!_pproducer) {
		throw std::invalid_argument(
			stringbuilder()
			<< "Failed to create new producer: " << errstr
		);
	}

	_ptopic = RdKafka::Topic::create(_pproducer, _target_topic, tconf, errstr);
	if(!_ptopic) {
		throw std::invalid_argument(
			stringbuilder()
			<< "Failed to create topic object: " << _target_topic << " Err: " << errstr
		);
	}
}

}; // namespace K3

