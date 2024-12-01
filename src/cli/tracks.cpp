/**
 * track segments calculator
 * @author Tobias Weber
 * @date 24 November 2024
 * @license see 'LICENSE' file
 *
 * g++ -std=c++20 -march=native -O2 -Wall -Wextra -Weffc++ -o track track.cpp
 */

#include "lib/trackdb.h"
#include "common/types.h"


int main(int argc, char **argv)
{
	if(argc <= 1)
	{
		std::cerr << "Please give a track file." << std::endl;
		return -1;
	}

	try
	{
		SingleTrack<t_real> track;
		if(!track.Import(argv[1]))
		{
			std::cerr << "Cannot load track file!" << std::endl;
			return -1;
		}

		std::cout << track << std::endl;
	}
	catch(const std::exception& ex)
	{
		std::cerr << ex.what() << std::endl;
		return -1;
	}

	return 0;
}
