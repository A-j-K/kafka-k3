
#include <string>
#include <stdexcept>
#include <exception>

#include "utils.hpp"
#include "kafkaconf.hpp"

KafkaConf::KafkaConf()
{}

KafkaConf::~KafkaConf()
{}

RdKafka::Conf* 
KafkaConf::create(json_t *pjson, RdKafka::Conf::ConfType intype)
{
	RdKafka::Conf *rval = NULL;
	if (json_is_object(pjson)) {
		json_t *pval;
		const char *pkey;
		if((rval = RdKafka::Conf::create(intype)) == NULL) {
			throw std::invalid_argument(
				stringbuilder()
				<< "Failed to create conf object at " << __LINE__
			);
		}
		json_object_foreach(pjson, pkey, pval) {
			std::string errstr;
			RdKafka::Conf::ConfResult result = 
				rval->set(pkey, json_string_value(pval), errstr);
			if(result != RdKafka::Conf::ConfResult::CONF_OK) {
				delete rval;
				rval = NULL;
				throw std::invalid_argument(
					stringbuilder()
					<< "Configure failure at line "
					<< __LINE__ << " " << errstr
				);
			}
		}
		
	}
	return rval;
}


