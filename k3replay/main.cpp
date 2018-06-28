
#include <string>
#include <cstdio>
#include <csignal>
#include <iostream>
#include <algorithm>

#include <jansson.h>

#include "s3.hpp"
#include "awsguard.hpp"
#include "messagetime.hpp"

static bool run_system;

static void
sigterm(int sig) 
{
	run_system = false;
}

static std::string
cleanup_name(const std::string & inprefix, const std::string & in)
{
	std::string rval = in.substr(inprefix.size());
	rval = rval.substr(0, rval.find("-rand"));
	return rval;
}

int
main(int argc, char *argv[], char **envp)
{
	int rval = 1;
	json_t *paws = NULL;

	std::vector<std::string> vlist;

	signal(SIGINT,  sigterm);
	signal(SIGTERM, sigterm);

	K3::AwsGuard aws;
	K3::S3 s3client;

	s3client.setup(paws, envp);

	std::string marker = "rmm.entity/2018-06-07-14-18-33.2395583572-rand1549680648";

	K3::MessageTime pivot("2018-06-07-14-42-32.2541676059");

	if(!s3client.list("rmm.entity/", vlist, 5, &marker)) {
		std::cout << "false" << std::endl;
	}
	else {
		std::cout << "Pivot:  " << pivot.to_string() << std::endl;
		for(auto itor = vlist.begin(); itor != vlist.end(); itor++) {
			std::string cleaned = cleanup_name("rmm.entity/", *itor);
			K3::MessageTime t(cleaned);

			std::cout << "Actual: " << t.to_string();
			std::cout << " test != " << ((t != pivot) ? " true " : " false ");
			std::cout << " test >  " << ((t > pivot) ? " true " : " false ");
			std::cout << " test <  " << ((t < pivot) ? " true " : " false ");

			std::cout << std::endl;


		}
	}



	std::cout << "Shutting down: " << rval << std::endl;
	return rval;
}

