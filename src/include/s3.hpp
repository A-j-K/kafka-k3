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
	typedef std::shared_ptr<Aws::S3::S3Client> ClientShPtr;
	typedef std::shared_ptr<Aws::StringStream> StreamShPtr;

	ClientShPtr	_sp3client;

	Aws::String	_access_key;
	Aws::String	_secret_key;
	Aws::String	_bucket;
	Aws::String	_region;
	Aws::String	_kms_arn;
	
	bool	_encrypted;

	virtual ClientShPtr
	createS3Client(const std::string &);

	virtual ClientShPtr
	createS3ClientEncrypted(const std::string &);


public:

	S3();
	virtual ~S3();

	S3(ClientShPtr);

	virtual S3& setRegion(const Aws::String& s)    { _region     = s; return *this; }
	virtual S3& setBucket(const Aws::String& s)    { _bucket     = s; return *this; }
	virtual S3& setKmsArn(const Aws::String& s)    { _kms_arn    = s; return *this; }
	virtual S3& setAccessKey(const Aws::String& s) { _access_key = s; return *this; }
	virtual S3& setSecretKey(const Aws::String& s) { _secret_key = s; return *this; }

	virtual const Aws::String getRegion()&    { return _region;     }
	virtual const Aws::String getBucket()&    { return _bucket;     }
	virtual const Aws::String getKmsArn()&    { return _kms_arn;    }
	virtual const Aws::String getAccessKey()& { return _access_key; }
	virtual const Aws::String getSecretKey()& { return _secret_key; }

	virtual void setup(json_t*, char **envp = NULL);

	virtual bool put(const char *payload, size_t len,  
		const std::string & s3key,
	        const Utils::Metadata & metadata);

	virtual bool list(const std::string &, std::vector<std::string> &);

	virtual bool get(const std::string & inkey, std::stringstream & outstream,
			Aws::Map<Aws::String, Aws::String> * meta = NULL);

	virtual bool get(const std::string & inkey, const std::string & infilename,
			Aws::Map<Aws::String, Aws::String> * meta = NULL);

};

}; // namespace K3

