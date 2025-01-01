/**
 * map loader testing
 * @author Tobias Weber (orcid: 0000-0002-7230-1932)
 * @date 14 December 2024
 * @license see 'LICENSE' file
 */

#include "common/types.h"
#include "lib/map.h"

#include <sstream>


int main(int argc, char **argv)
{
	if(argc <= 2)
	{
		std::cerr << "Please give an osm input and an svg output file name.\n"
			<< "Options:\n"
			<< "\t--xml    \t\tuse internal xml loader\n"
			<< "\t--scale 1\t\tsvg scaling factor\n"
			<< std::endl;
		return -1;
	}

	bool use_xml_loader = false;
	t_real svg_scale = 1.;
	t_real min_lon = -10., max_lon = 10.;
	t_real min_lat = -10., max_lat = 10.;

	for(int i = 1; i < argc; ++i)
	{
		if(std::string(argv[i]) == "--xml")
			use_xml_loader = true;
		else if(std::string(argv[i]) == "--scale" && i + 1 < argc)
			std::istringstream{argv[i+1]} >> svg_scale;
	}

	try
	{
		Map<t_real, t_size> map;
		map.SetSkipBuildings(false);
		map.SetSkipLabels(true);

		if(use_xml_loader && !map.ImportXml(argv[1]))
		{
			std::cerr << "Could not read \"" << argv[1] << "\"." << std::endl;
			return -1;
		}

		if(!use_xml_loader && !map.Import(argv[1], min_lon, max_lon, min_lat, max_lat))
		{
			std::cerr << "Could not read \"" << argv[1] << "\"." << std::endl;
			return -1;
		}

		if(!map.ExportSvg(argv[2], svg_scale))
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
