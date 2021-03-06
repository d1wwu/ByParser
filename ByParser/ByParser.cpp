#include "pch.h"
#include <iostream>
#include "program.h"


void print_usage()
{
	std::cout << "byparser [-p] <cutting_plan> [-h]" << std::endl;
	std::cout << std::endl;
	std::cout << "  -h     		Prints this usage message." << std::endl;
	std::cout << "  -p <cutting_plan>	Cutting plan." << std::endl;
}

int main(int argc, char *argv[])
{
	const char *plan_name = nullptr;
	cmap::const_iterator it;

	for (int i = 1; i < argc; i++)
	{
		char *arg = argv[i];
		if (strcmp(arg, "-h") == 0) {
			print_usage();
			exit(0);
		}
		else if (strcmp(arg, "-p") == 0) {
			if (++i < argc)
				plan_name = argv[i];
			else
				std::cerr << "A file name must follow after -p." << std::endl;
		}
		else {
			std::cerr << "Unknown flag '" << arg << "'." << std::endl;
			exit(1);
		}
	}

	if (!plan_name) {
		print_usage();
	}
	else {
		CPlan plan(plan_name);

		for (it = plan.calc_map_.begin(); it != plan.calc_map_.end(); ++it)
		{
			std::cout << (it->first).c_str() << ": " << (it->second).c_str() << std::endl;
		}
	}

	return 0;
}
