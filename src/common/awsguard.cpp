
#include <aws/core/Aws.h>
#include <aws/core/utils/logging/LogMacros.h>
#include <aws/core/utils/logging/AWSLogging.h>
#include <aws/core/utils/logging/ConsoleLogSystem.h>
#include <aws/core/Globals.h>

#include "awsguard.hpp" 

#ifndef ALLOCATION_TAG
#define ALLOCATION_TAG "create_function_console_logging"
#endif

namespace K3 {

Aws::Utils::Logging::LogLevel AwsGuard::_loglevel = Aws::Utils::Logging::LogLevel::Info;

AwsGuard::AwsGuard() 
{
	_options.loggingOptions.logLevel = Aws::Utils::Logging::LogLevel::Off;
	Aws::InitAPI(_options);
	Aws::Utils::Logging::InitializeAWSLogging(
		Aws::MakeShared<Aws::Utils::Logging::ConsoleLogSystem>(
			ALLOCATION_TAG, Aws::Utils::Logging::LogLevel::Info));
}

AwsGuard::AwsGuard(json_t *inp) 
{
	json_t *p;
	Aws::Utils::Logging::LogLevel loglevel = Aws::Utils::Logging::LogLevel::Info;

	_options.loggingOptions.logLevel = Aws::Utils::Logging::LogLevel::Off;
	Aws::InitAPI(_options);

	if((p = json_object_get(inp, "loglevel")) != NULL ) {
		if(json_is_string(p)) {
			std::string s(json_string_value(p));
			if(s == "off") {
				loglevel = Aws::Utils::Logging::LogLevel::Off;
			}
			else if(s == "fatal") {
				loglevel = Aws::Utils::Logging::LogLevel::Fatal;
			}
			else if(s == "error") {
				loglevel = Aws::Utils::Logging::LogLevel::Error;
			}
			else if(s == "warn") {
				loglevel = Aws::Utils::Logging::LogLevel::Warn;
			}
			else if(s == "info") {
				loglevel = Aws::Utils::Logging::LogLevel::Info;
			}
			else if(s == "debug") {
				loglevel = Aws::Utils::Logging::LogLevel::Debug;
			}
			else if(s == "trace") {
				loglevel = Aws::Utils::Logging::LogLevel::Trace;
			}
		}
	}

	AwsGuard::setLoglevel(loglevel);

	Aws::Utils::Logging::InitializeAWSLogging(
		Aws::MakeShared<Aws::Utils::Logging::ConsoleLogSystem>(
			ALLOCATION_TAG, loglevel));
}

AwsGuard::~AwsGuard()
{
	Aws::ShutdownAPI(_options);
}

}; // namespace K3
