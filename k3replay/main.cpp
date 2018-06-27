
#include <string>
#include <cstdio>
#include <csignal>
#include <iostream>

static bool run_system;

static void
sigterm(int sig) 
{
	run_system = false;
}

int
main(int argc, char *argv[])
{
	int rval = 1;
	signal(SIGINT,  sigterm);
	signal(SIGTERM, sigterm);
	std::cout << "Shutting down: " << rval << std::endl;
	return rval;
}

