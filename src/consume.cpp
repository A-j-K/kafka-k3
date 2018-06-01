
#include <string>
#include <vector>
#include <chrono>
#include <random>
#include <ostream>
#include <iostream>
#include <algorithm>
#include <stdexcept>
#include <exception>

#include "utils.hpp"
#include "consume.hpp"
#include "kafkaconf.hpp"
#include "messagewrapper.hpp"

namespace K3 {

Consume::Consume() :
	_plog(&std::cout),
	_ppenv(0),
	_consume_wait_time(1000),
	_message_bundle_limit(10),
	_message_bundle_size(1024 * 1024 * 10)
{}

Consume::~Consume()
{
	if(_pconf) delete _pconf;
	if(_pconsumer) {
		_pconsumer->close();
		delete _pconsumer;
	}
	RdKafka::wait_destroyed(5000);
}

bool 
Consume::topic_excluded(std::string & topic)
{
	auto itor = _exclude_topics.begin();
	while(itor != _exclude_topics.end()) {
		if(Utils::strcmp(*itor, topic)) {
			return true;
		}
		itor++;
	}
	return false;
}

void
Consume::setup(json_t *pjson)
{
	const char *s;
	std::string errstr;
	std::vector<std::string> topics;

	auto p = json_object_get(pjson, "exclude_topics");
	if(p && json_is_array(p)) {
		size_t index;
		json_t *pval;	
		json_array_foreach(p, index, pval) {
			if(json_is_string(pval)) {
				_exclude_topics.push_back(std::string(json_string_value(pval)));
			}
			else if(json_is_object(pval)) {
				auto pname = json_object_get(pval, "name");
				if(pname) {
					_exclude_topics.push_back(std::string(json_string_value(pname)));
				}
			}
		}
	}

	p = json_object_get(pjson, "topics");
	if(p && json_is_array(p)) {
		size_t index;
		json_t *pval;	
		json_array_foreach(p, index, pval) {
			if(json_is_string(pval)) {
				std::string str_topic(json_string_value(pval));
				if(!topic_excluded(str_topic)) {
					topics.push_back(str_topic);
				}
			}
			else if(json_is_object(pval)) {
				auto pname = json_object_get(pval, "name");
				if(pname) {
					std::string str_topic(json_string_value(pname));
					if(!topic_excluded(str_topic)) {
						topics.push_back(str_topic);
					}
				}
			}
		}
	}

	p = json_object_get(pjson, "general");
	if(p && json_is_object(p)) {
		json_t *ptemp;
		if((ptemp = json_object_get(p, "batchsize"))) {
			if(json_is_integer(ptemp)) {
				setMessageBundleLimit(json_integer_value(ptemp));
			}
		}
	}

	p = json_object_get(pjson, "default_global_conf");
	if(p && json_is_object(p)) {
		KafkaConf helper;
		_pconf = helper.create(p, RdKafka::Conf::CONF_GLOBAL);
	}
	else {
		throw std::invalid_argument(
			stringbuilder()
			<< "Failed to create a configuration"
		);
	}

	// Allow ENV vars to override config file if they exist.
	if((s = std::getenv("KAFKA_GROUP_ID")) != NULL) {
		_pconf->set("group.id", std::string(s), errstr);
	}
	if((s = std::getenv("KAFKA_SECURITY_PROTOCOL")) != NULL) {
		_pconf->set("security.protocol", std::string(s), errstr);
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
	if((s = std::getenv("KAFKA_MESSAGE_BATCHSIZE")) != NULL) {
		int i = atoi(s);
		setMessageBundleLimit(i);
	}

	p = json_object_get(pjson, "default_topic_conf");
	if(p && json_is_object(p)) {
		KafkaConf helper;
		RdKafka::Conf *tconf = NULL;
		if((tconf = helper.create(p, RdKafka::Conf::CONF_TOPIC)) != NULL) {
			_pconf->set("default_topic_conf", tconf, errstr);
			delete tconf;
		}
	}

	_pconf->set("enable.auto.commit", "false", errstr);

	_pconsumer = RdKafka::KafkaConsumer::create(_pconf, errstr);
	if(!_pconsumer) {
		throw std::invalid_argument(
			stringbuilder()
			<< "Failed to start consumer: " << errstr 
		);
	}

	if(topics.size() < 1) {
		// Attempt auto discovery of topics.
		RdKafka::Metadata *pmetadata = NULL;
		auto result = _pconsumer->metadata(true, NULL, &pmetadata, 10000);
		if(pmetadata != NULL && result == RdKafka::ErrorCode::ERR_NO_ERROR) {
			const RdKafka::Metadata::TopicMetadataVector *ptopics = pmetadata->topics();
			if(ptopics) {
				std::string comp("__consumer_offsets");
				auto comp_size = comp.size();
				RdKafka::Metadata::TopicMetadataIterator itor = ptopics->begin();
				while(itor != ptopics->end()) {
					auto ptopic = *itor;
					auto t = ptopic->topic();
					auto s = t.substr(0, comp_size);
					if(s != comp && !topic_excluded(t)) {
						topics.push_back(t);
					}
					itor++;
				}
			}
		}
		if(result != RdKafka::ErrorCode::ERR_NO_ERROR) {
			*_plog << "Failed during metadata request to discover topics" << std::endl;
		}
		if(pmetadata != NULL) {
			delete pmetadata;
		}
	}

	if(topics.size() > 0) {
		auto itor = topics.begin();
		*_plog << "Subscribing to topics:-" << std::endl;
		while(itor != topics.end()) {
			*_plog << "   " << *itor << std::endl;
			itor++;

		}
		_pconsumer->subscribe(topics);
	}
	else {
		throw std::invalid_argument(
			stringbuilder()
			<< "No topics provided or discovered at "
			<< __LINE__ << " in function " << __FUNCTION__
		); 
	}
}

size_t 
Consume::messagesSize(MessageVector &messages)
{
	size_t rval = 0;
	if(messages.size() > 0) {
		MessageVector::iterator itor = messages.begin();
		while(itor != messages.end()) {
			rval += (*itor)->getMessage()->len();
			itor++;
		}
	}
	return rval;
}

int64_t
Consume::messageChecksum(const char *inp, size_t len) 
{
	int i = 0, j = 0;
	while(len--) {
		i += (int)((unsigned char)inp[j++]);
	}
	return i;
}

void
Consume::run(bool *loop_control)
{
	while(*loop_control != false) {
		RdKafka::Message *pmsg = NULL;
		if((pmsg = _pconsumer->consume(getConsumeWaitTime())) != NULL) {
			std::string topic_name;
			switch(pmsg->err()) {
			case RdKafka::ERR_NO_ERROR: 
				topic_name = pmsg->topic_name();
				_messages[pmsg->topic_name()].push_back(
					MessageWrapper::ShPtr(new MessageWrapper(_pconsumer, pmsg))
				);
				{
					MessageMapSize::const_iterator itor = _messageSizes.find(topic_name);
					if(itor == _messageSizes.end()) {
						_messageSizes[topic_name] = 0;
					}
					_messageSizes[topic_name] += pmsg->len();
				}
				if(_messages[pmsg->topic_name()].size() >= getMessageBundleLimit()) {
					stash_by_topic(topic_name.c_str(), _messages[topic_name]);
					_messageSizes[topic_name] = 0;
				}
				else {
					if(_messageSizes[topic_name] >= getMessageBundleSize()) {
						stash_by_topic(topic_name.c_str(), _messages[topic_name]);
						_messageSizes[topic_name] = 0;
					}
				}
				break;
			default:
			case RdKafka::ERR__TIMED_OUT: 
			case RdKafka::ERR__PARTITION_EOF:
				stash_all();
				delete pmsg;
				break;
			} // switch ends
		}
	}

	stash_all(); 
}

void
Consume::stash_all()
{
	if(_messages.size() > 0) {
		for(MessageMap::iterator itor = _messages.begin(); 
		    itor != _messages.end();
		    itor++) 
		{
			stash_by_topic(itor->first.c_str(), itor->second);
		}
	}
}

void 
Consume::stash_by_topic(const char *topic, MessageVector &messages)
{
	// Note, using S3 multipart uploads would have been much better
	// and more memory efficient. However, each multipart is a PUT
	// operation and we are "bulking up" messages into a single PUT
	// to avoid the cost of a PUT per message. So using multipart
	// PUTs would have just doubled the number per message (for the
	// header also). So we build an in-ram image and use a single PUT.
	//
	// Each message consists of a header (of fixed size) and the 
	// message content after the header. The header contains the
	// size of the message to expect and various other pieces of
	// information about the message.
	if(messages.size() > 0) {
		int64_t total_payload_size = messagesSize(messages);
		if(total_payload_size > 0) {
			int index_counter = messages.size() - 1;
			int64_t offset = 0;
			int64_t total_header_size = sizeof(MessageHeader) * messages.size();
			int64_t total_size = total_header_size + total_payload_size;
			char *p = new char[(total_size + 1) * 2];
			if(p && _ps3 != NULL) {
				int64_t last_ts = 0;
				std::string s3key;
				Utils::Metadata metadata;
				memset((void*)p, 0, (total_size + 1) * 2);
				for(auto itor = messages.begin(); itor != messages.end(); itor++) {
					MessageHeader *phead = (MessageHeader*)(p + offset);
					RdKafka::Message *pmsg = (*itor)->getMessage();
					size_t payload_len = (*itor)->getMessage()->len();
					const char *payload = (const char*)(*itor)->getMessage()->payload();
					phead->set(pmsg, index_counter--,
						messageChecksum(payload, payload_len));
					char *body = (char*)(p + sizeof(MessageHeader) + offset);
					memcpy(body, payload, payload_len);
					last_ts = pmsg->timestamp().timestamp;
					offset += sizeof(MessageHeader) + payload_len;
				}
				{
					char buffer[256];
					auto t  = (time_t)(last_ts / 1000);
					auto tm = localtime(&t);
					memset(buffer, 0, sizeof(buffer));
					strftime(buffer, sizeof(buffer), "%Y-%m-%d-%H-%M-%S", tm);
					unsigned seed = std::chrono::system_clock::now().time_since_epoch().count();
					std::minstd_rand0 generator(seed);
					std::string key(stringbuilder()
						<< topic << "/" << buffer
						<< "." << seed
						<< "-rand" << generator());
					metadata["x-numofmsgs"] = stringbuilder() << messages.size();
					_ps3->put(p, total_size, key, metadata);
				}
			}
			else {
				*_plog << "Failed to create RAM image, " << total_size << " bytes not available" << std::endl;
			}
			if(p) delete [] p;
		}
		messages.clear();
	}
}

}; // namespace K3

