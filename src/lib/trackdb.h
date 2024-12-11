/**
 * track files loader
 * @author Tobias Weber (orcid: 0000-0002-7230-1932)
 * @date 24 November 2024
 * @license see 'LICENSE' file
 */

#ifndef __TRACK_DBFILE_H__
#define __TRACK_DBFILE_H__

#include "track.h"

#include <algorithm>
#include <map>


#define TRACKDB_MAGIC "TRACKDB"



/**
 * represents a collection of running tracks
 */
template<class t_real = double, class t_size = std::size_t>
class MultipleTracks
{
public:
	using t_track = SingleTrack<t_real, t_size>;
	using t_TrackPoint = t_track::t_TrackPoint;
	using t_clk = typename t_track::t_clk;
	using t_timept = typename t_track::t_timept;


public:
	MultipleTracks() = default;
	~MultipleTracks() = default;


	t_track* GetTrack(t_size idx)
	{
		if(idx >= GetTrackCount())
			return nullptr;

		return &m_tracks[idx];
	}


	const t_track* GetTrack(t_size idx) const
	{
		if(idx >= GetTrackCount())
			return nullptr;

		return &m_tracks[idx];
	}


	void EmplaceTrack(t_track&& track)
	{
		m_tracks.emplace_back(std::forward<t_track>(track));
		m_tracks.rbegin()->SetDistanceFunction(m_distance_function);
	}


	void AddTrack(const t_track& track)
	{
		m_tracks.push_back(track);
		m_tracks.rbegin()->SetDistanceFunction(m_distance_function);
	}


	t_size GetTrackCount() const
	{
		return m_tracks.size();
	}


	void ClearTracks()
	{
		m_tracks.clear();
	}


	void DeleteTrack(t_size idx)
	{
		if(idx >= GetTrackCount())
			return;

		m_tracks.erase(m_tracks.begin() + idx);
	}


	/**
	 * sort tracks by time stamp
	 */
	void SortTracks()
	{
		std::stable_sort(m_tracks.begin(), m_tracks.end(),
			[](const t_track& track1, const t_track& track2) -> bool
		{
			auto t1 = track1.GetStartTime();
			auto t2 = track2.GetStartTime();

			if(!t1 || !t2)
				return false;

			return *t1 >= *t2;
		});
	}


	/**
	 * calculate track properties
	 */
	void Calculate()
	{
		for(t_track& track : m_tracks)
			track.Calculate();
	}


	bool Save(const std::string& filename) const
	{
		using t_pos = typename std::ofstream::pos_type;
		std::ofstream ofstr{filename, std::ios::binary};
		if(!ofstr)
			return false;

		const t_size num_tracks = GetTrackCount();

		ofstr.write(TRACKDB_MAGIC, sizeof(TRACKDB_MAGIC));
		ofstr.write(reinterpret_cast<const char*>(&num_tracks), sizeof(num_tracks));
		t_pos pos_addresses = ofstr.tellp();
		ofstr.seekp(num_tracks * sizeof(t_size), std::ios::cur);

		for(t_size trackidx = 0; trackidx < num_tracks; ++trackidx)
		{
			const t_track* track = GetTrack(trackidx);
			if(!track)
				return false;

			t_pos pos_before = ofstr.tellp();
			if(!track->Save(ofstr))
				return false;
			t_pos pos_after = ofstr.tellp();

			// write track start address
			t_size pos_track = static_cast<t_size>(pos_before);
			ofstr.seekp(pos_addresses + static_cast<t_pos>(trackidx*sizeof(t_size)), std::ios::beg);
			ofstr.write(reinterpret_cast<const char*>(&pos_track), sizeof(pos_track));

			ofstr.seekp(pos_after, std::ios::beg);
		}

		return true;
	}


	bool Load(const std::string& filename)
	{
		ClearTracks();

		using t_pos = typename std::ifstream::pos_type;
		std::ifstream ifstr{filename, std::ios::binary};
		if(!ifstr)
			return false;

		char magic[sizeof(TRACKDB_MAGIC)];
		ifstr.read(magic, sizeof(magic));
		if(std::string(magic) != TRACKDB_MAGIC)
			return false;

		t_size num_tracks = 0;
		ifstr.read(reinterpret_cast<char*>(&num_tracks), sizeof(num_tracks));
		m_tracks.reserve(num_tracks);

		t_pos pos_addresses = ifstr.tellg();

		for(t_size trackidx = 0; trackidx < num_tracks; ++trackidx)
		{
			// read track start address
			t_size pos_track = 0;
			ifstr.seekg(pos_addresses + static_cast<t_pos>(trackidx*sizeof(t_size)), std::ios::beg);
			ifstr.read(reinterpret_cast<char*>(&pos_track), sizeof(pos_track));

			// seek to track
			ifstr.seekg(static_cast<t_pos>(pos_track), std::ios::beg);

			t_track track{};
			track.SetDistanceFunction(m_distance_function);
			if(!track.Load(ifstr))
				return false;

			m_tracks.emplace_back(std::move(track));
		}

		SortTracks();
		return true;
	}


	void SetDistanceFunction(int dist_func)
	{
		m_distance_function = dist_func;

		for(t_track& track : m_tracks)
			track.SetDistanceFunction(m_distance_function);
	}


	/**
	 * total distance of all tracks
	 */
	t_real GetTotalDistance(bool planar = false) const
	{
		t_real dist{};

		for(const t_track& track : m_tracks)
			dist += track.GetTotalDistance(planar);

		return dist;
	}


	/**
	 * distance of all tracks sorted by months
	 */
	std::map<t_timept, t_real> GetDistancePerMonth(bool planar = false) const
	{
		std::map<t_timept, t_real> map;

		for(const t_track& track : m_tracks)
		{
			std::optional<t_timept> tp = track.GetStartTime();
			if(!tp)
				continue;
			t_timept tp_rd = round_timepoint<t_clk, t_timept>(*tp);

			t_real dist = track.GetTotalDistance(planar);
			if(auto iter = map.find(tp_rd); iter != map.end())
				iter->second += dist;
			else
				map.insert(std::make_pair(tp_rd, dist));
		}

		return map;
	}


private:
	std::vector<t_track> m_tracks{};

	int m_distance_function{0};
};


#endif
