#pragma once

#include <memory>
#include <jansson.h>
#include <librdkafka/rdkafkacpp.h>

class KafkaConf
{
public:
	typedef std::shared_ptr<RdKafka::Conf> RdKafkaConf;

	KafkaConf();
	virtual ~KafkaConf();
	virtual RdKafka::Conf* create(json_t*, RdKafka::Conf::ConfType);
};

