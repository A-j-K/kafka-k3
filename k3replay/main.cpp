
#include <string>
#include <cstdio>
#include <csignal>
#include <iostream>
#include <algorithm>

static bool run_system;

static void
sigterm(int sig) 
{
	run_system = false;
}

int
main(int argc, char *argv[], char **envp)
{
	int rval = 1;
	signal(SIGINT,  sigterm);
	signal(SIGTERM, sigterm);


       if(envp) {
                std::string comp("RDKAFKA_SETVAR_");
                auto comp_size = comp.size();
                for(char **env = envp; *env != 0; env++) {
                       	std::string fullenvname(*env);
                        std::string subenvname = fullenvname.substr(0, comp.size());
                        if(subenvname == comp) {
                                std::string envname = fullenvname.substr(comp.size());
				std::string key = envname.substr(0, envname.find("="));
				std::string val = envname.substr(key.size()+1);
				std::replace(key.begin(), key.end(), '_', '.');
				std::transform(key.begin(), key.end(), key.begin(), ::tolower);
				std::cout << fullenvname << std::endl;
				std::cout << envname << std::endl;
				std::cout << key << std::endl;
				std::cout << val << std::endl;
                        }
                }
        }
	else {
		std::cout << "No envp" << std::endl;
	}



	std::cout << "Shutting down: " << rval << std::endl;
	return rval;
}

