#pragma once

#include <jansson.h>
#include <aws/core/Aws.h>
#include <aws/core/utils/logging/AWSLogging.h>

namespace K3 {

class AwsGuard 
{
protected:
	Aws::SDKOptions _options;

public:

	AwsGuard();
	AwsGuard(json_t*);
	virtual ~AwsGuard();

	static Aws::Utils::Logging::LogLevel _loglevel;
	
	static void 
	setLoglevel(Aws::Utils::Logging::LogLevel l) {
		_loglevel = l;
	}

	static Aws::Utils::Logging::LogLevel
	getLoglevel() {
		return _loglevel;
	}
};

}; // namespace K3
