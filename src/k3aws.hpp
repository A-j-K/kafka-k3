#pragma once

#include <aws/core/Aws.h>

class K3Aws 
{
protected:
	Aws::SDKOptions _options;

public:

	K3Aws();
	virtual ~K3Aws();
};
