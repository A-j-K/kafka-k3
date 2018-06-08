
#include "messagewrapper.hpp"

namespace K3 {

MessageWrapper::MessageWrapper() : 
	_pconsumer(0), _pmessage(0) 
{}

MessageWrapper::MessageWrapper(RdKafka::KafkaConsumer *pc, RdKafka::Message *pm) :
	_pconsumer(pc), _pmessage(pm) 
{};

MessageWrapper::~MessageWrapper() {
	if(_pmessage && _pconsumer) {
		_pconsumer->commitAsync(_pmessage);
		delete _pmessage;
	}
}

RdKafka::KafkaConsumer*
MessageWrapper::getConsumer() 
{ 
	return _pconsumer; 
}

RdKafka::Message*
MessageWrapper::getMessage() 
{
	return _pmessage; 
}

}; // namespace K3

