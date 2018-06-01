#pragma once

#include <map>
#include <vector>
#include <iostream>
#include <jansson.h>
#include <librdkafka/rdkafkacpp.h>

#include "utils.hpp"
#include "messagewrapper.hpp"
#include <aws/s3/S3Client.h>
#include <aws/core/auth/AWSCredentialsProviderChain.h>

namespace K3 {

class S3
{
protected:
	Aws::Auth::AWSCredentials		*_pcreds;
	Aws::Client::ClientConfiguration	*_pconfig;
	Aws::S3::S3Client			*_ps3client;

	Aws::String			_access_key;
	Aws::String			_secret_key;
	Aws::String			_bucket;
	Aws::String			_region;
	Aws::String			_kms_arn;
	
	bool	_delete;
	bool	_encrypted;

	//std::vector<MessageWrapper::ShPtr>	_messages;

	std::ostream	*_plog;

public:

	S3();
	virtual ~S3();

	S3(Aws::Auth::AWSCredentials*,
		Aws::Client::ClientConfiguration*,
		Aws::S3::S3Client*);

	virtual S3& setRegion(const Aws::String& s)    { _region     = s; return *this; }
	virtual S3& setBucket(const Aws::String& s)    { _bucket     = s; return *this; }
	virtual S3& setKmsArn(const Aws::String& s)    { _kms_arn    = s; return *this; }
	virtual S3& setAccessKey(const Aws::String& s) { _access_key = s; return *this; }
	virtual S3& setSecretKey(const Aws::String& s) { _secret_key = s; return *this; }

	virtual Aws::String getRegion()    { return _region;     }
	virtual Aws::String getBucket()    { return _bucket;     }
	virtual Aws::String getKmsArn()    { return _kms_arn;    }
	virtual Aws::String getAccessKey() { return _access_key; }
	virtual Aws::String getSecretKey() { return _secret_key; }

	virtual bool prepare();

	virtual void setup(json_t*);

	virtual bool put(const char *payload, size_t len,  
		std::string & s3key,
	        std::map<std::string, std::string> & metadata);

	virtual bool put(std::shared_ptr<char>&, size_t len,  
		std::string & s3key,
	        std::map<std::string, std::string> & metadata);
};

}; // namespace K3

