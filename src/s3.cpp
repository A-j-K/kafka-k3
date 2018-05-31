
#include "s3.hpp" 

#include <ctime>
#include <cstdlib>

#include <aws/core/http/Scheme.h>
#include <aws/s3/model/CreateBucketRequest.h>
#include <aws/s3-encryption/S3EncryptionClient.h>
#include <aws/s3-encryption/CryptoConfiguration.h>
#include <aws/core/auth/AWSCredentialsProviderChain.h>
#include <aws/s3-encryption/materials/KMSEncryptionMaterials.h>

using namespace Aws::S3;
using namespace Aws::Auth;
using namespace Aws::S3::Model;
using namespace Aws::S3Encryption;
using namespace Aws::S3Encryption::Materials;

S3::S3() :
	_pcreds(0),
	_pconfig(0),
	_ps3client(0),
	_delete(true),
	_encrypted(false),
	_plog(&std::cout)
{
	const char *p;
	_region = "eu-west-1"; // default region
	if((p = std::getenv("AWS_REGION")) != NULL) _region = p;
	if((p = std::getenv("AWS_BUCKET")) != NULL) _bucket = p;
	if((p = std::getenv("AWS_KMS_ARN")) != NULL) _kms_arn = p;
	if((p = std::getenv("AWS_ACCESS_KEY")) != NULL) _access_key = p;
	if((p = std::getenv("AWS_SECRET_KEY")) != NULL) _secret_key = p;
}

S3::S3(Aws::Auth::AWSCredentials *creds,
	Aws::Client::ClientConfiguration *cc,
	Aws::S3::S3Client *sc) :
		_pcreds(creds),
		_pconfig(cc),
		_ps3client(sc),
		_delete(false),
		_encrypted(false),
		_plog(&std::cout)
{}


S3::~S3()
{
	if(_delete) {
		if(_ps3client) delete _ps3client;
		if(_pconfig) delete _pconfig;
		if(_pcreds) delete _pcreds;
	}
}


bool 
S3::prepare()
{
	_pcreds = new Aws::Auth::AWSCredentials(getAccessKey(), getSecretKey());
	_pconfig = new Aws::Client::ClientConfiguration;
	_pconfig->region = getRegion();

	if(getKmsArn().size() == 0) {
		_ps3client = new S3Client(*_pcreds, *_pconfig);
	}
	else {
		auto kmsMaterials = Aws::MakeShared<KMSEncryptionMaterials>("", getKmsArn());
		CryptoConfiguration crypto_configuration(
			StorageMethod::METADATA,
			CryptoMode::STRICT_AUTHENTICATED_ENCRYPTION);
		_ps3client = new S3EncryptionClient(kmsMaterials,
			crypto_configuration, *_pcreds, *_pconfig);		
		_encrypted = true;
	}

	return true;
}

void
S3::setup(json_t *pjson)
{
	json_t *p;
	if(!json_is_object(pjson)) return;
	if((p = json_object_get(pjson, "region")) != NULL) _region = json_string_value(p);
	if((p = json_object_get(pjson, "bucket")) != NULL) _bucket = json_string_value(p);
	if((p = json_object_get(pjson, "kms_arn")) != NULL) _kms_arn = json_string_value(p);
	if((p = json_object_get(pjson, "access_key")) != NULL) _access_key = json_string_value(p);
	if((p = json_object_get(pjson, "secret_key")) != NULL) _secret_key = json_string_value(p);
	prepare();
}

bool 
S3::put(RdKafka::Message &inmsg, const char *pkey)
{
	bool rval = false;
	Aws::String msg_key;
	Aws::StringStream offset;
	Aws::StringStream partition;
	Aws::StringStream timestamp;
	auto requestStream = getKmsArn().size() == 0 ?
		Aws::MakeShared<Aws::StringStream>("s3") :
		Aws::MakeShared<Aws::StringStream>("s3Encryption");
	*requestStream << inmsg.payload();
	msg_key.append((char*)inmsg.key_pointer(), (size_t)inmsg.key_len());
	offset << inmsg.offset();
	partition << inmsg.partition();
	timestamp << inmsg.timestamp().timestamp;
	PutObjectRequest putObjectRequest;
	putObjectRequest.WithBucket(getBucket())
		.AddMetadata("X-Kafka-Key", msg_key)
		.AddMetadata("X-Kafka-Topic", inmsg.topic_name())
		.AddMetadata("X-Kafka-Partition", partition.str())
		.AddMetadata("X-Kafka-Offset", offset.str())
		.AddMetadata("X-Kafka-Timmestamp", timestamp.str())
		.AddMetadata("X-Encrypted", _encrypted ? getKmsArn() : "0")
		.SetBody(requestStream);
	if(!pkey) {
		char buffer[64];
		Aws::StringStream key;
		time_t t  = (time_t)(inmsg.timestamp().timestamp / 1000);
		auto   tm = localtime(&t);
		memset(buffer, 0, sizeof(buffer));
		strftime(buffer, sizeof(buffer), "%Y-%m-%d-%H-%M", tm);
		key << inmsg.topic_name() << "/"
			<< buffer << "/" 
			<< partition.str() << "-"
			<< msg_key << "-"
			<< offset.str() << "-" 
			<< inmsg.timestamp().timestamp;
		putObjectRequest.WithKey(key.str());
	}
	else {
		putObjectRequest.WithKey(pkey);
	}
	auto putObjectOutcome = _ps3client->PutObject(putObjectRequest);
	if((rval = putObjectOutcome.IsSuccess()) == false) {
		*_plog << "Error while putting Object " 
                << putObjectOutcome.GetError().GetExceptionName() 
                << " " 
                << putObjectOutcome.GetError().GetMessage() 
                << std::endl;
	}
	return rval;
}

bool 
S3::put(MessageWrapper::ShPtr & insp)
{
	_messages.push_back(insp);

	return true;
}

bool
S3::put(std::shared_ptr<char> &insp, size_t len, 
	std::string & s3key,
	std::map<std::string, std::string> & metadata)
{
	return put(insp.get(), len, s3key, metadata);
}

bool 
S3::put(const char *payload, size_t len, 
	std::string & s3key,
	std::map<std::string, std::string> & metadata)
{
	bool rval = false;
	Aws::String sload;
	PutObjectRequest putObjectRequest;
	std::map<std::string, std::string>::iterator metadata_itor = metadata.begin();

	auto requestStream = _encrypted ?
		Aws::MakeShared<Aws::StringStream>("s3Encryption") :
		Aws::MakeShared<Aws::StringStream>("s3");
	sload.append(payload, len);
	*requestStream << sload;
	putObjectRequest.WithBucket(getBucket());
	while(metadata_itor != metadata.end()) {
		putObjectRequest.AddMetadata(metadata_itor->first, metadata_itor->second);
		metadata_itor++;
	}
	putObjectRequest.WithKey(s3key);
	putObjectRequest.SetBody(requestStream);
	if(_ps3client) {
		auto putObjectOutcome = _ps3client->PutObject(putObjectRequest);
		if((rval = putObjectOutcome.IsSuccess()) == false) {
			*_plog << "Error while putting Object " 
	                << putObjectOutcome.GetError().GetExceptionName() 
        	        << " " 
                	<< putObjectOutcome.GetError().GetMessage() 
	                << std::endl;
		}
	}
	return rval;
}


