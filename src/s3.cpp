
#include "s3.hpp" 

#include <ctime>
#include <cstdlib>

#include <aws/core/http/Scheme.h>
#include <aws/s3/model/CreateBucketRequest.h>
#include <aws/core/utils/logging/LogMacros.h>
#include <aws/s3-encryption/S3EncryptionClient.h>
#include <aws/s3-encryption/CryptoConfiguration.h>
#include <aws/core/auth/AWSCredentialsProviderChain.h>
#include <aws/s3-encryption/materials/KMSEncryptionMaterials.h>

namespace K3 {

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
	_bucket = "k3dump";    // default bucket name
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
	_pconfig = new Aws::Client::ClientConfiguration;
	_pconfig->region = getRegion();

	if(getKmsArn().size() == 0) {
		if(getAccessKey().size() > 0) {
			_pcreds = new Aws::Auth::AWSCredentials(getAccessKey(), getSecretKey());
			_ps3client = new Aws::S3::S3Client(*_pcreds, *_pconfig);
		}
		else {
			auto pcreds = Aws::MakeShared<Aws::Auth::DefaultAWSCredentialsProviderChain>("");
			_ps3client = new Aws::S3::S3Client(pcreds, *_pconfig);
		}
	}
	else {
		auto kmsMaterials = Aws::MakeShared<Aws::S3Encryption::Materials::KMSEncryptionMaterials>("", getKmsArn());
		Aws::S3Encryption::CryptoConfiguration crypto_configuration(
			Aws::S3Encryption::StorageMethod::METADATA,
			Aws::S3Encryption::CryptoMode::STRICT_AUTHENTICATED_ENCRYPTION);
		if(getAccessKey().size() > 0) {	
			_pcreds = new Aws::Auth::AWSCredentials(getAccessKey(), getSecretKey());
			_ps3client = new Aws::S3Encryption::S3EncryptionClient(kmsMaterials,
				crypto_configuration, *_pcreds, *_pconfig);		
		}
		else {
			auto pcreds = Aws::MakeShared<Aws::Auth::DefaultAWSCredentialsProviderChain>("");
			_ps3client = new Aws::S3Encryption::S3EncryptionClient(kmsMaterials,
				crypto_configuration, pcreds, *_pconfig);
		}
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
	if(_ps3client) {
		Aws::S3::Model::PutObjectRequest putObjectRequest;
		putObjectRequest.WithKey(s3key);
		putObjectRequest.WithBucket(getBucket());
		auto requestStream = _encrypted ?
			Aws::MakeShared<Aws::StringStream>("s3Encryption") :
			Aws::MakeShared<Aws::StringStream>("s3");
		requestStream->write(payload, len);
		putObjectRequest.SetBody(requestStream);
		for(auto metadata_itor = metadata.begin();
			metadata_itor != metadata.end();
			metadata_itor++)
		{
			putObjectRequest.AddMetadata(metadata_itor->first, metadata_itor->second);
		}
		AWS_LOGSTREAM_DEBUG("K3-PUT", "Putting object to S3 bucket '" << getBucket() << "'");
		auto putObjectOutcome = _ps3client->PutObject(putObjectRequest);
		if((rval = putObjectOutcome.IsSuccess()) == false) {
			AWS_LOGSTREAM_INFO("K3-PUT",
				"Error while putting Object; ExceptionName: "
				<< putObjectOutcome.GetError().GetExceptionName()
				<< "; Message: " << putObjectOutcome.GetError().GetMessage()
			);
		}
		else {
			AWS_LOGSTREAM_DEBUG("K3-PUT", "Put object to S3 bucket '" << getBucket() << "' Success");
		}
	}
	else {
		AWS_LOGSTREAM_WARN("K3-PUT", "Failed to PUT, no S3 client defined at line " << __LINE__ << " in file " << __FILE__);
	}
	return rval;
}

}; // namespace K3

