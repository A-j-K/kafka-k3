
#include <string>
#include <cstdio>
#include <csignal>
#include <iostream>
#include <jansson.h>

#include "s3.hpp"
#include "k3aws.hpp"
#include "consume.hpp"

static bool run_system;
static std::string conffile;

static void
sigterm(int sig) 
{
	run_system = false;
}

static int
comsume_to_s3(bool *run)
{
	int rval = -1;
	K3Aws aws;
	json_error_t jerr;
	json_t *pconf, *paws, *pkafka;

	if((pconf = json_load_file(conffile.c_str(), 0, &jerr)) == NULL) {
		std::cout << jerr.text << std::endl 
			<< jerr.source 
			<< " at line: " << jerr.line 
			<< " col: " << jerr.column 
			<< std::endl;
		return -1;
	}

	paws = json_object_get(pconf, "aws");
	pkafka = json_object_get(pconf, "kafka");
	
	S3 s3client;
	Consume consumer;
	s3client.setup(paws);
	consumer.setup(pkafka);
	consumer.setS3client(&s3client);
	consumer.run(&run_system);
	rval = 0;
	json_decref(pconf);
	return rval;
}

int
main(int argc, char *argv[])
{
	int rval = 0;

	signal(SIGINT,  sigterm);
	signal(SIGTERM, sigterm);

	conffile = "/etc/k3conf.json";
	for(int i = 1; i < argc; i++) {
		if(std::string(argv[i]) == "-f") {
			if(i < argc) {
				conffile = argv[i+1];
			}
		}
	}

	run_system = true;
	rval = comsume_to_s3(&run_system);

	std::cout << "Shutting down" << std::endl;
	return rval;
}

