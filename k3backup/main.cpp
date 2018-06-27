
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

int
main(int argc, char *argv[], char **envp)
{
	int rval = 1;
	json_t *pconf = NULL;
	json_error_t jerr;
	std::string conffile;

	signal(SIGINT,  sigterm);
	signal(SIGTERM, sigterm);

	if((pconf = json_load_file("/etc/k3conf.json", 0, &jerr)) == NULL) {
		std::cout << "Tried /etc/k3conf.json: " << jerr.text << std::endl 
			<< jerr.source 
			<< " at line: " << jerr.line 
			<< " col: " << jerr.column 
			<< std::endl 
			<< " trying /etc/k3backup.json" << std::endl;
		if((pconf = json_load_file("/etc/k3backup.json", 0, &jerr)) == NULL) {
			std::cout << "Tried /etc/k3backup.json: " << jerr.text << std::endl 
				<< jerr.source 
				<< " at line: " << jerr.line 
				<< " col: " << jerr.column 
				<< std::endl;
		}
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
			std::cout << "Trying " << conffile << std::endl;
			if((pconf = json_load_file(conffile.c_str(), 0, &jerr)) == NULL) {
				std::cout << "Tried " << conffile << ": " << jerr.text << std::endl 
					<< jerr.source 
					<< " at line: " << jerr.line 
					<< " col: " << jerr.column 
					<< std::endl;
			}
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

