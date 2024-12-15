/**
 * map loader testing
 * @author Tobias Weber (orcid: 0000-0002-7230-1932)
 * @date 14 December 2024
 * @license see 'LICENSE' file
 */

#include "common/types.h"
#include "lib/map.h"


int main(int argc, char **argv)
{
	if(argc <= 2)
	{
		std::cerr << "Please give an osm input and an svg output file name." << std::endl;
		return -1;
	}

	try
	{
		Map<t_real, t_size> map;

		if(!map.Import(argv[1]))
		{
			std::cerr << "Could not read \"" << argv[1] << "\"." << std::endl;
			return -1;
		}

		if(!map.ExportSvg(argv[2]))
		{
			std::cerr << "Could not write \"" << argv[2] << "\"." << std::endl;
			return -1;
		}
	}
	catch(const std::exception& ex)
	{
		std::cerr << ex.what() << std::endl;
		return -1;
	}

	return 0;
}
