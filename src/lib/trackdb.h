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
#include <tuple>
#include <vector>
#include <string_view>
#include <concepts>
#include <thread>
#include <mutex>
#include <future>
#include <boost/asio.hpp>


#define TRACKDB_MAGIC "TRACKDB"



/**
 * represents a collection of running tracks
 */
template<class t_real = double, class t_size = std::size_t>
requires std::floating_point<t_real> && std::integral<t_size>
class MultipleTracks
{
public:
	using t_track = SingleTrack<t_real, t_size>;
	using t_clk = typename t_track::t_clk;
	using t_timept = typename t_track::t_timept;
	using t_timept_map = std::map<t_timept,
		std::tuple<t_real /*dist*/, t_real /*time*/, t_size /*# tracks*/>>;



public:
	MultipleTracks()
		: m_num_threads{std::max<unsigned int>(std::thread::hardware_concurrency() / 2, 1)}
	{
	}



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
		m_tracks.rbegin()->SetAscentEpsilon(m_asc_eps);
		m_tracks.rbegin()->SetSmoothRadius(m_smooth_rad);
	}



	void AddTrack(const t_track& track)
	{
		m_tracks.push_back(track);
		m_tracks.rbegin()->SetDistanceFunction(m_distance_function);
		m_tracks.rbegin()->SetAscentEpsilon(m_asc_eps);
		m_tracks.rbegin()->SetSmoothRadius(m_smooth_rad);
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

		//std::cout << "Deleting track index " << idx << ": " << GetTrack(idx)->GetFileName() << std::endl;
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
		boost::asio::thread_pool tp{m_num_threads};

		for(t_track& track : m_tracks)
		{
			boost::asio::post(tp, [&track]() -> void
			{
				track.Calculate();
			});
		}

		tp.join();
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
		if(std::string_view(magic) != TRACKDB_MAGIC)
			return false;

		t_size num_tracks = 0;
		ifstr.read(reinterpret_cast<char*>(&num_tracks), sizeof(num_tracks));
		m_tracks.reserve(num_tracks);

		t_pos pos_addresses = ifstr.tellg();

		boost::asio::thread_pool tp{m_num_threads};
		std::vector<std::shared_ptr<std::packaged_task<std::optional<t_track>()>>> tasks;
		tasks.reserve(num_tracks);

		for(t_size trackidx = 0; trackidx < num_tracks; ++trackidx)
		{
			auto task_func = [this, &filename, pos_addresses, trackidx]() -> std::optional<t_track>
			{
				std::ifstream ifstr_track{filename, std::ios::binary};
				if(!ifstr_track)
					return std::nullopt;

				// read track start address
				t_size pos_track = 0;
				ifstr_track.seekg(pos_addresses + static_cast<t_pos>(trackidx*sizeof(t_size)), std::ios::beg);
				ifstr_track.read(reinterpret_cast<char*>(&pos_track), sizeof(pos_track));

				// seek to track
				ifstr_track.seekg(static_cast<t_pos>(pos_track), std::ios::beg);

				t_track track{};
				track.SetDistanceFunction(m_distance_function);
				track.SetAscentEpsilon(m_asc_eps);
				track.SetSmoothRadius(m_smooth_rad);

				if(!track.Load(ifstr_track))
					return std::nullopt;

				return track;
			};

			auto task = std::make_shared<std::packaged_task<std::optional<t_track>()>>(task_func);
			boost::asio::post(tp, [task]() -> void { (*task)(); });
			tasks.push_back(task);
		}

		for(auto& task : tasks)
		{
			auto track = task->get_future().get();
			if(track)
				m_tracks.emplace_back(std::move(*track));
		}

		tp.join();
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
	 * minimum height difference [m] before being counted as climb
	 */
	void SetAscentEpsilon(t_real eps)
	{
		m_asc_eps = eps;

		for(t_track& track : m_tracks)
			track.SetAscentEpsilon(m_asc_eps);
	}



	/**
	 * number of neighbouring points to include in data smoothing
	 */
	void SetSmoothRadius(t_size rad)
	{
		m_smooth_rad = rad;

		for(t_track& track : m_tracks)
			track.SetSmoothRadius(m_smooth_rad);
	}



	void SetNumThreads(unsigned int num)
	{
		m_num_threads = num;
	}



	/**
	 * total distance of all tracks
	 */
	t_real GetTotalDistance(bool planar = false) const
	{
		boost::asio::thread_pool tp{m_num_threads};
		std::vector<std::shared_ptr<std::packaged_task<t_real()>>> tasks;
		tasks.reserve(m_tracks.size());

		for(const t_track& track : m_tracks)
		{
			auto task_func = [&track, planar]() -> t_real
			{
				return track.GetTotalDistance(planar);
			};

			auto task = std::make_shared<std::packaged_task<t_real()>>(task_func);
			boost::asio::post(tp, [task]() -> void { (*task)(); });
			tasks.push_back(task);
		}

		t_real dist{};
		for(auto& task : tasks)
			dist += task->get_future().get();

		tp.join();
		return dist;
	}



	/**
	 * distance of all tracks sorted by months
	 */
	t_timept_map GetDistancePerPeriod(bool planar = false, bool yearly = false) const
	{
		t_timept_map map;
		std::mutex mtx;

		boost::asio::thread_pool tp{m_num_threads};

		for(const t_track& track : m_tracks)
		{
			boost::asio::post(tp, [&track, &map, &mtx, planar, yearly]() -> void
			{
				std::optional<t_timept> tpt = track.GetStartTime();
				if(!tpt)
					return;
				t_timept tpt_rd = round_timepoint<t_clk, t_timept>(*tpt, yearly);

				t_real dist = track.GetTotalDistance(planar);
				t_real time = track.GetTotalTime();

				std::lock_guard lck{mtx};

				if(auto iter = map.find(tpt_rd); iter != map.end())
				{
					std::get<0>(iter->second) += dist;  // distance
					std::get<1>(iter->second) += time;  // time
					++std::get<2>(iter->second);        // track counter
				}
				else
				{
					map.insert(std::make_pair(tpt_rd, std::make_tuple(dist, time, 1)));
				}
			});
		}

		tp.join();
		return map;
	}



	/**
	 * print an overview of the tracks contained in this database
	 */
	friend std::ostream& operator<<(std::ostream& ostr, const MultipleTracks<t_real, t_size>& tracks)
	{
		const int idx_width = 6;
		const int field_width = 25;
		const int name_width = 45;

		// header
		ostr
			<< std::left << std::setw(idx_width) << "Number" << " "
			<< std::left << std::setw(field_width) << "Dist." << " "
			<< std::left << std::setw(field_width) << "Time" << " "
			<< std::left << std::setw(name_width) << "Name" << "\n";

		for(t_size idx = 0; idx < tracks.GetTrackCount(); ++idx)
		{
			const t_track* track = tracks.GetTrack(idx);

			ostr
				<< std::left << std::setw(idx_width) << idx + 1 << " "
				<< std::left << std::setw(field_width) << get_dist_str(track->GetTotalDistance()) << " "
				<< std::left << std::setw(field_width) << get_time_str(track->GetTotalTime()) << " "
				<< std::left << std::setw(name_width) << track->GetFileName() << "\n";
		}

		return ostr;
	}




private:
	std::vector<t_track> m_tracks{};

	int m_distance_function{0};

	t_real m_asc_eps{5.};
	t_size m_smooth_rad{10};

	unsigned int m_num_threads = 4;
};


#endif
