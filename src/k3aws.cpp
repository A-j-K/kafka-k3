
#include "k3aws.hpp" 

K3Aws::K3Aws() 
{
	_options.loggingOptions.logLevel = Aws::Utils::Logging::LogLevel::Trace;
	Aws::InitAPI(_options);
}

K3Aws::~K3Aws()
{
	Aws::ShutdownAPI(_options);
}

