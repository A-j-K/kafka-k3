
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
	_encrypted(false)
{
	_region = "eu-west-1"; // default region
	_bucket = "k3dump";    // default bucket name
	AWS_LOGSTREAM_INFO("K3-ctor", "S3 starting up");
}

S3::S3(ClientShPtr sc) :
	_sp3client(sc),
	_encrypted(false)
{
	AWS_LOGSTREAM_INFO("K3-ctor", "S3 starting up (test mode)");
}


S3::~S3()
{
	AWS_LOGSTREAM_INFO("K3-dtor", "S3 shutting down");
}

S3::ClientShPtr
S3::createS3Client(const std::string & in_access_key)
{
	ClientShPtr rval;
	Aws::Client::ClientConfiguration config;
	config.region = getRegion();
	if(in_access_key.size() > 0) {
		AWS_LOGSTREAM_INFO("K3-PREPARE", "Creating non-encrypted S3 client with user supplied creds");
		Aws::Auth::AWSCredentials creds(getAccessKey(), getSecretKey());
		rval = Aws::MakeShared<Aws::S3::S3Client>("k3-client-maunual-auth", creds, config);
	}
	else {
		AWS_LOGSTREAM_INFO("K3-PREPARE", "Creating non-encrypted S3 client with default creds provider");
		auto creds = Aws::MakeShared<Aws::Auth::DefaultAWSCredentialsProviderChain>("k3-creds-default-auth");
		rval = Aws::MakeShared<Aws::S3::S3Client>("k3-client-default-auth", creds, config);
	}
	_encrypted = false;
	return rval;
}

S3::ClientShPtr
S3::createS3ClientEncrypted(const std::string & in_access_key)
{
	ClientShPtr rval;
	Aws::Client::ClientConfiguration config;
	auto materials = Aws::MakeShared<Aws::S3Encryption::Materials::KMSEncryptionMaterials>("k3-client-kms", getKmsArn());
	config.region = getRegion();
	Aws::S3Encryption::CryptoConfiguration crypto_configuration(
		Aws::S3Encryption::StorageMethod::METADATA,
		Aws::S3Encryption::CryptoMode::STRICT_AUTHENTICATED_ENCRYPTION);
	if(in_access_key.size() > 0) {	
		AWS_LOGSTREAM_INFO("K3-PREPARE", "Creating encrypted S3 client with user supplied creds");
		Aws::Auth::AWSCredentials creds(getAccessKey(), getSecretKey());
		rval = Aws::MakeShared<Aws::S3Encryption::S3EncryptionClient>("k3-client-manual-auth-enc",
			materials, crypto_configuration, creds, config);
		
	}
	else {
		AWS_LOGSTREAM_INFO("K3-PREPARE", "Creating encrypted S3 client with default creds provider");
		auto creds = Aws::MakeShared<Aws::Auth::DefaultAWSCredentialsProviderChain>("k3-creds-default-auth-enc");
		rval = Aws::MakeShared<Aws::S3Encryption::S3EncryptionClient>("k3-client-default-auth-enc",
			materials, crypto_configuration, creds, config);
	}
	_encrypted = true;
	return rval;
}

void
S3::setup(json_t *pjson, char **penv)
{
	const char *pe;

	AWS_LOGSTREAM_INFO("K3-SETUP", "Configuring");

	if(pjson && json_is_object(pjson)) { 
		json_t *p;
		if((p = json_object_get(pjson, "region")) != NULL)
			_region = json_string_value(p);
		if((p = json_object_get(pjson, "bucket")) != NULL)
			_bucket = json_string_value(p);
		if((p = json_object_get(pjson, "kms_arn")) != NULL)
			_kms_arn = json_string_value(p);
		if((p = json_object_get(pjson, "access_key")) != NULL)
			_access_key = json_string_value(p);
		if((p = json_object_get(pjson, "secret_key")) != NULL)
			_secret_key = json_string_value(p);
	}

	// ENV vars can override
	if((pe = std::getenv("AWS_REGION")) != NULL)
		_region = pe;
	if((pe = std::getenv("AWS_BUCKET")) != NULL)
		_bucket = pe;
	if((pe = std::getenv("AWS_KMS_ARN")) != NULL)
		_kms_arn = pe;
	if((pe = std::getenv("AWS_ACCESS_KEY")) != NULL)
		_access_key = pe;
	if((pe = std::getenv("AWS_SECRET_KEY")) != NULL)
		_secret_key = pe;

	AWS_LOGSTREAM_INFO("K3-SETUP", "Region: " << _region);
	AWS_LOGSTREAM_INFO("K3-SETUP", "Bucket: " << _bucket);
	AWS_LOGSTREAM_INFO("K3-SETUP", "Access: " << _access_key);
	AWS_LOGSTREAM_INFO("K3-SETUP", "KmsArn: " << _kms_arn);

	_sp3client = (getKmsArn().size() == 0) ?
		createS3Client(getAccessKey()) :
		createS3ClientEncrypted(getAccessKey());
}

bool 
S3::put(const char *payload, size_t len, 
	const std::string & s3key,
	const Utils::Metadata & metadata)
{
	bool rval = false;
	if(_sp3client.get()) {
		Aws::S3::Model::PutObjectRequest request;
		auto body = Aws::MakeShared<Aws::StringStream>(_encrypted ? "s3Encryption" : "s3");
		body->write(payload, len);
		request.WithKey(s3key);
		request.WithBucket(getBucket());
		request.SetBody(body);
		request.SetContentLength(len);
		request.SetContentType("binary/octet-stream");
		for(auto metadata_itor = metadata.begin();
			metadata_itor != metadata.end();
			metadata_itor++)
		{
			request.AddMetadata(metadata_itor->first, metadata_itor->second);
		}
		AWS_LOGSTREAM_DEBUG("K3-PUT", 
			"Putting object to S3 bucket '" << getBucket() << "'");
		auto outcome = _sp3client->PutObject(request);
		if((rval = outcome.IsSuccess()) == false) {
			AWS_LOGSTREAM_WARN("K3-PUT",
				"Error while putting Object; ExceptionName: "
				<< outcome.GetError().GetExceptionName()
				<< "; Message: " << outcome.GetError().GetMessage()
			);
		}
		else {
			AWS_LOGSTREAM_DEBUG("K3-PUT", 
				"Put object to S3 bucket '" << getBucket() << "' Success");
		}
	}
	else {
		AWS_LOGSTREAM_WARN("K3-PUT", 
			"Failed to PUT, no S3 client defined at line " 
			<< __LINE__ << " in file " << __FILE__);
	}
	return rval;
}

}; // namespace K3

