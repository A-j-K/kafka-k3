
#include <aws/core/Aws.h>
#include <aws/core/utils/logging/LogMacros.h>
#include <aws/core/utils/logging/AWSLogging.h>
#include <aws/core/utils/logging/ConsoleLogSystem.h>

#include <aws/core/Globals.h>

#include "k3aws.hpp" 

#ifndef ALLOCATION_TAG
#define ALLOCATION_TAG "create_function_console_logging"
#endif

K3Aws::K3Aws() 
{
	_options.loggingOptions.logLevel = Aws::Utils::Logging::LogLevel::Off;
	Aws::InitAPI(_options);
	Aws::Utils::Logging::InitializeAWSLogging(
		Aws::MakeShared<Aws::Utils::Logging::ConsoleLogSystem>(
			ALLOCATION_TAG, Aws::Utils::Logging::LogLevel::Info));
}

K3Aws::~K3Aws()
{
	Aws::ShutdownAPI(_options);
}


