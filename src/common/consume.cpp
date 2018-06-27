
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
#include "memchecker.hpp"
#include "messagewrapper.hpp"

namespace K3 {

Consume::Consume() :
	_plog(&std::cout),
	_ppenv(0),
	_mem_percent(.85),
	_rebalance_called(false),
	_consume_wait_time(1000),
	_message_bundle_limit(10),
	_message_bundle_size(1024 * 1024 * 10)
{}

Consume::~Consume()
{
	stash_all();
	if(_pconsumer) {
		_pconsumer->close();
		delete _pconsumer;
	}
	if(_pconf) delete _pconf;
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
Consume::setup(json_t *pjson, char **envp)
{
	const char *s;
	std::string errstr;
	std::vector<std::string> topics;

	if(pjson) {
		setup_exclude_topics(pjson);
		setup_topics(pjson, topics);
		setup_general(pjson);
		setup_default_global_conf(pjson);
		setup_default_topic_conf(pjson);
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

	// Attempt to find all ENV VARs that begin "RDKAFKA_SET_" and then apply them.
	// Example: "RDKAFKA_SET_METADATA_BROKER_LIST=foo.com" becomes "metadata.broker.list"
	// and it's value ::set("metadata.broker.list", "foo.com")
	if(envp) {
		std::string comp("RDKAFKA_SETVAR_");
		for(char **env = envp; *env != 0; env++) {
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
				std::string comp("__"); // Don't backup topics that start with this.
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

int
Consume::run(bool *loop_control)
{
	int mem_test_counter = 0;
	MemChecker mem(_mem_percent);
	while(*loop_control != false) {
		if(false && _rebalance_called) {
			// Actually, this is a bad thing to do because a rebalance will
			// happen if one of the k3 instances is restarted by K8s or a mem
			// failure. So we need to be smarter here about discovering new topics.
			*_plog << "Kafka rebalance took place, restarting..." << std::endl;
			return 2;
		}
		if(++mem_test_counter > 30) {
			mem_test_counter = 0;
			if(!mem.ok()) {
				*_plog << "Over assigned memory limit, restarting..." << std::endl;
				return 1; 
			}
		}
		run_once();
	}
	return 0;
}

void
Consume::run_once()
{
	RdKafka::Message *pmsg = NULL;
	if((pmsg = _pconsumer->consume(getConsumeWaitTime())) != NULL) {
		std::string topic_name;
		switch(pmsg->err()) {
		case RdKafka::ERR_NO_ERROR: 
			topic_name = pmsg->topic_name();
			_messages[topic_name].push_back(
				MessageWrapper::ShPtr(new MessageWrapper(_pconsumer, pmsg))
			);
			{
				MessageMapSize::const_iterator itor = _messageSizes.find(topic_name);
				if(itor == _messageSizes.end()) {
					_messageSizes[topic_name] = 0;
				}
				_messageSizes[topic_name] += pmsg->len();
			}
			if(_messages[topic_name].size() >= getMessageBundleLimit()) {
				stash_by_topic(topic_name.c_str(), _messages[topic_name]);
				_messageSizes[topic_name] = 0;
			}
			else {
				if(_messageSizes[topic_name] >= getMessageBundleSize()) {
					stash_by_topic(topic_name.c_str(), _messages[topic_name]);
					_messageSizes[topic_name] = 0;
				}
			}
			// We don't delete pmsg here as it's ownership is assigned to
			// a MessageWrapper::ShPtr which in turn deletes it (and during 
			// delete the commitAsync() is called to ensure it's committed)
			// when the _messages map is cleared during stash.
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

void
Consume::stash_all()
{
	if(_messages.size() < 1) return;
	for(auto itor = _messages.begin(); itor != _messages.end(); itor++) {
		stash_by_topic(itor->first.c_str(), itor->second);
	}
}

void 
Consume::stash_by_topic(const char *topic, MessageVector &messages)
{
	if(messages.size() < 1) return;

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
			if(true) { // code block for temp vars.
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
	auto itor = messages.begin();
	while(itor != messages.end()) {
		itor = messages.erase(itor);
	}
}

void
Consume::setup_exclude_topics(json_t *pjson)
{
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

}

void
Consume::setup_topics(json_t *pjson, std::vector<std::string> & topics)
{
	auto p = json_object_get(pjson, "topics");
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
}

void
Consume::setup_general(json_t *pjson)
{
	auto p = json_object_get(pjson, "general");
	if(p && json_is_object(p)) {
		json_t *ptemp;
		if((ptemp = json_object_get(p, "batchsize"))) {
			if(json_is_integer(ptemp)) {
				setMessageBundleLimit(json_integer_value(ptemp));
			}
		}
		if((ptemp = json_object_get(p, "binsize"))) {
			if(json_is_integer(ptemp)) {
				setMessageBundleSize(json_integer_value(ptemp));
			}
		}
		if((ptemp = json_object_get(p, "collecttime_ms"))) {
			if(json_is_integer(ptemp)) {
				setConsumeWaitTime(json_integer_value(ptemp));
			}
		}
		if((ptemp = json_object_get(p, "mem_percent"))) {
			if(json_is_real(ptemp)) {
				double percent = json_real_value(ptemp);
				if(percent < 1) {
					setMemPercent(percent);
				}
			}
		}
	}
}

void
Consume::setup_default_global_conf(json_t *pjson)
{
	auto p = json_object_get(pjson, "default_global_conf");
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

}

void
Consume::setup_default_topic_conf(json_t *pjson)
{
	std::string errstr;
	auto p = json_object_get(pjson, "default_topic_conf");
	if(_pconf && p && json_is_object(p)) {
		KafkaConf helper;
		RdKafka::Conf *tconf = NULL;
		if((tconf = helper.create(p, RdKafka::Conf::CONF_TOPIC)) != NULL) {
			_pconf->set("default_topic_conf", tconf, errstr);
			delete tconf;
		}
		else {
			throw std::invalid_argument(
				stringbuilder()
				<< "Failed to create a topic configuration: "
				<< errstr 
				<< " at line " << __LINE__ 
				<< " in file " << __FILE__
			);
		}
	}
}

}; // namespace K3

