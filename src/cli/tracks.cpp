/**
 * track segments calculator
 * @author Tobias Weber (orcid: 0000-0002-7230-1932)
 * @date 24 November 2024
 * @license see 'LICENSE' file
 */

#include "lib/trackdb.h"
#include "common/types.h"


namespace fs = std::filesystem;


static bool load_tracks(const fs::path& file, const std::optional<t_size>& track_idx)
{
	try
	{
		MultipleTracks<t_real> tracks;
		if(!tracks.Load(file.string()))
		{
			std::cerr << "Could not read " << file << "." << std::endl;
			return false;
		}

		if(track_idx)
		{
			// output a single track
			const auto* track = tracks.GetTrack(*track_idx);
			if(track)
				std::cout << *track << std::endl;
			else
				std::cerr << "Invalid track number." << std::endl;
		}
		else
		{
			// output track list
			std::cout << tracks << std::endl;
		}
	}
	catch(const std::exception& ex)
	{
		std::cerr << ex.what() << std::endl;
		return false;
	}

	return true;
}


/**
 * fix shifted track names due to delete bug in gui
 */
static bool fix_track_names(const fs::path& file, t_size start_idx = 0)
{
	try
	{
		MultipleTracks<t_real> tracks;
		if(!tracks.Load(file.string()))
		{
			std::cerr << "Could not read " << file << "." << std::endl;
			return false;
		}

		for(int idx = (int)tracks.GetTrackCount() - 1; idx >= (int)start_idx; --idx)
		{
			std::string prev_name = "<unnamed>";
			if(idx > 0)
			{
				const auto* prev_track = tracks.GetTrack(idx - 1);
				prev_name = prev_track->GetFileName();
			}

			auto* track = tracks.GetTrack(idx);
			track->SetFileName(prev_name);
		}

		std::string fixed = "fixed.tracks";
		if(!tracks.Save(fixed))
		{
			std::cerr << "Could not save \"" << fixed << "\"." << std::endl;
			return false;
		}
	}
	catch(const std::exception& ex)
	{
		std::cerr << ex.what() << std::endl;
		return false;
	}

	return true;
}


static bool load_gpx(const fs::path& file)
{
	try
	{
		SingleTrack<t_real> track;
		if(!track.Import(file.string()))
		{
			std::cerr << "Could not read " << file << "." << std::endl;
			return false;
		}

		std::cout << track << std::endl;
	}
	catch(const std::exception& ex)
	{
		std::cerr << ex.what() << std::endl;
		return false;
	}

	return true;
}


int main(int argc, char **argv)
{
	if(argc <= 1)
	{
		std::cerr << "Please give a .tracks or a .gpx track file." << std::endl;
		return -1;
	}

	bool do_fix = false;

	// was a track index given as second argument?
	std::optional<t_size> track_idx;
	if(argc > 2)
		track_idx = std::stoul(argv[2]) - 1;

	// file name as first argument
	std::filesystem::path file{argv[1]};
	if(!fs::exists(file))
	{
		std::cerr << "File " << file << " does not exist." << std::endl;
		return -1;
	}

	if(file.extension() == ".tracks")
	{
		if(do_fix)
			fix_track_names(file, *track_idx);
		else
			load_tracks(file, track_idx);
	}
	else if(file.extension() == ".gpx")
	{
		load_gpx(file);
	}
	else
	{
		std::cerr << "Unknown file format." << std::endl;
		return -1;
	}

	return 0;
}
