/**
 * track segments calculator
 * @author Tobias Weber (orcid: 0000-0002-7230-1932)
 * @date 24 November 2024
 * @license see 'LICENSE' file
 */

#include "lib/trackdb.h"
#include "common/types.h"


int main(int argc, char **argv)
{
	if(argc <= 1)
	{
		std::cerr << "Please give a gpx track file." << std::endl;
		return -1;
	}

	try
	{
		SingleTrack<t_real> track;
		if(!track.Import(argv[1]))
		{
			std::cerr << "Could not read \"" << argv[1] << "\"." << std::endl;
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
