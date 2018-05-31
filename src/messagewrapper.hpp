#pragma once

#include <memory>
#include <string>
#include <sstream>
#include <cstdint>

#include <librdkafka/rdkafkacpp.h>

class MessageWrapper
{
public:
	typedef std::shared_ptr<MessageWrapper> ShPtr;

protected:
	RdKafka::KafkaConsumer	*_pconsumer;
	RdKafka::Message	*_pmessage;

public:
	MessageWrapper(); 
	MessageWrapper(RdKafka::KafkaConsumer *pc, RdKafka::Message *pm);
	virtual ~MessageWrapper();

	virtual RdKafka::KafkaConsumer*
	getConsumer();

	virtual RdKafka::Message*
	getMessage();
};

#pragma pack(push, 1)
struct MessageHeaderDetails
{
	int64_t	header_version;
	int64_t	payload_len;	
	int64_t	partition;
	int64_t	offset;
	int64_t	timestamp;
	int64_t	index;
	int64_t csum;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct MessageHeader
{
	const static int HeaderSize = 1024;
	const static int KeyMaxLen = 128;
	const static int TopicMaxLen = (HeaderSize - sizeof(MessageHeaderDetails) - KeyMaxLen);

	MessageHeaderDetails details;
	const char key_field[KeyMaxLen];
	const char topic_name[TopicMaxLen];

	void set(RdKafka::Message *pmsg, int64_t in_index, int64_t in_csum) {
		int len;
		memset(this, 0, sizeof(MessageHeader));
		details.header_version = 1;
		details.timestamp      = pmsg->timestamp().timestamp;
		details.partition      = pmsg->partition();
		details.offset         = pmsg->offset();
		details.index          = in_index;
		details.csum           = in_csum;
		details.payload_len    = pmsg->len();

		if((len = pmsg->key_len()) > KeyMaxLen-1) len = KeyMaxLen-1;
		memcpy((void*)&key_field[0], pmsg->key_pointer(), len);

		if((len = pmsg->topic_name().size()) > (TopicMaxLen-1)) len = TopicMaxLen - 1;
		memcpy((void*)&topic_name[0], pmsg->topic_name().c_str(), len);
	}
};
#pragma pack(pop)

