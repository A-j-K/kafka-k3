
#include <string>
#include <cstdio>
#include <csignal>
#include <iostream>
#include <jansson.h>

#include "s3.hpp"
#include "awsguard.hpp"
#include "consume.hpp"

static bool run_system;

static void
sigterm(int sig) 
{
	run_system = false;
}

static int
comsume_to_s3(bool *run, json_t *pconf, char **envp)
{
	int rval = -1;
	json_t *paws = NULL, *pkafka = NULL;

	if(pconf) {
		paws = json_object_get(pconf, "aws");
		pkafka = json_object_get(pconf, "kafka");
	}
	
	K3::AwsGuard aws(paws);
	K3::S3 s3client;
	K3::Consume consumer;
	s3client.setup(paws, envp);
	consumer.setup(pkafka, envp);
	consumer.setS3client(&s3client);
	rval = consumer.run(&run_system);
	return rval;
}

static json_t*
get_config_file(const std::string & confile)
{
	json_t *pconf = NULL;
	json_error_t jerr;

	std::cout << "Try loading " << confile << ": ";
	if((pconf = json_load_file(confile.c_str(), 0, &jerr)) == NULL) {
		std::cout << jerr.text << std::endl;
		if(jerr.line > -1 && jerr.column > -1) { 
			std::cout << jerr.source 
				<< " at line: " << jerr.line 
				<< " col: " << jerr.column ;
		}
	}
	else {
		std::cout << "loaded";
	}
	std::cout << std::endl;
	return pconf;
}

int
main(int argc, char *argv[], char **envp)
{
	int rval = 1;
	json_t *pconf = NULL;
	json_error_t jerr;
	std::string conffile;

	signal(SIGINT,  sigterm);
	signal(SIGTERM, sigterm);

	pconf = get_config_file(std::string("/etc/k3conf.json"));
	if(pconf == NULL) {
		pconf = get_config_file(std::string("/etc/k3backup.json"));
	}
	if(pconf == NULL) {
		for(int i = 1; i < argc; i++) {
			if(std::string(argv[i]) == "-f") {
				if(i < argc) {
					conffile = argv[i+1];
				}
			}
		}
		if(conffile.size() < 1) {
			std::cout << "No configuration found to load\n";
		}
		else {
			pconf = get_config_file(conffile);
		}
	}

	if(pconf == NULL) {
		std::cout << "Continuing to load without a JSON config file, assuming ENV VARs available" << std::endl;
	}

	run_system = true;

	// Zero indicates normal termination.
	// Negative indicates fatal error.
	// Positive indicates restart system requested.
	while(rval > 0) {
		rval = comsume_to_s3(&run_system, pconf, envp);
	}

	json_decref(pconf);

	std::cout << "Shutting down: " << rval << std::endl;
	return rval;
}

