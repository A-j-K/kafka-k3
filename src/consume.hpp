
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

class Consume 
{
public:
	typedef std::vector<MessageWrapper::ShPtr> MessageVector;
	typedef std::map<std::string, MessageVector> MessageMap;
	typedef std::map<std::string, int64_t> MessageMapSize;
protected:
	S3			*_ps3;
	std::ostream    	*_plog;
        RdKafka::Conf		*_pconf;
        RdKafka::KafkaConsumer	*_pconsumer;

	const char 		**_ppenv;
	
	int			_message_bundle_size;
	int			_message_bundle_limit;
	int			_consume_wait_time;
	int			_auto_discover_interval = 60;
	bool			_auto_discover_topics = false;
	bool			_rebalance_called;
	double			_mem_percent;

	std::vector<std::string>	_exclude_topics;
	std::vector<std::string>	_topics;

	MessageMap 	_messages;
	MessageMapSize	_messageSizes;

	virtual bool 
	topic_excluded(std::string & topic);

	virtual bool
	topic_already_in_subscriptions(const std::string & topic);

	virtual void
	setup_exclude_topics(json_t*);

	virtual void
	setup_topics(json_t*);

	virtual void
	setup_general(json_t*);

	virtual void
	setup_default_global_conf(json_t*);

	virtual void
	setup_default_topic_conf(json_t*);

	virtual void
	auto_discover_topics();

public:
	Consume();
	virtual ~Consume();

	virtual Consume&
	setAutoDiscoverTopics(bool auto_discover_topics) {
		_auto_discover_topics = auto_discover_topics;
		return *this;
	}

	virtual bool
	getAutoDiscoverTopics() {
		return _auto_discover_topics;
	}

	virtual Consume& setAutoDiscoverInterval(int auto_discover_interval) {
		_auto_discover_interval = auto_discover_interval;
		return *this;
	}

	virtual int getAutoDiscoverInterval() {
		return _auto_discover_interval;
	}

	virtual Consume&
	setLogStream(std::ostream *pstream) {
		_plog = pstream;
		return *this;
	}

	virtual Consume&
	setS3client(S3 *p) {
		_ps3 = p;
		return *this;
	}

	virtual S3*
	getS3client() {
		return _ps3;
	}

	virtual Consume&
	setConsumeWaitTime(int i) {
		_consume_wait_time = i;
		return *this;
	}

	virtual int
	getConsumeWaitTime() {
		return _consume_wait_time;
	}

	virtual Consume&
	setMessageBundleLimit(int i) {
		_message_bundle_limit = i;
		return *this;
	}

	virtual int
	getMessageBundleLimit() {
		return _message_bundle_limit;
	}

	virtual Consume&
	setMessageBundleSize(int i) {
		_message_bundle_size = i;
		return *this;
	}

	virtual int
	getMessageBundleSize() {
		return _message_bundle_size;
	}

	virtual double
	getMemPercent() {
		return _mem_percent;
	}

	virtual Consume&
	setMemPercent(double d) {
		_mem_percent = d;
		return *this;
	}

	virtual void setup(json_t*, char **envp = NULL);
	virtual int run(bool *);

protected:

	virtual size_t messagesSize(MessageVector&);

	virtual int64_t messageChecksum(const char *, size_t);

	virtual void run_once();
	virtual void stash_all();
	virtual void stash_by_topic(const char*, MessageVector&);
};

}; // namespace K3

