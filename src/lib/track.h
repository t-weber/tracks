/**
 * track file loader
 * @author Tobias Weber (orcid: 0000-0002-7230-1932)
 * @date 24 November 2024
 * @license see 'LICENSE' file
 */

#ifndef __TRACK_FILE_H__
#define __TRACK_FILE_H__

#include <iostream>
#include <sstream>
#include <fstream>
#include <iomanip>
#include <limits>
#include <cmath>
#include <numbers>
#include <vector>
#include <concepts>

#if __has_include(<filesystem>)
	#include <filesystem>
	namespace __gpx_fs = std::filesystem;
#else
	#include <boost/filesystem.hpp>
	namespace __gpx_fs = boost::filesystem;
#endif

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>
#include <boost/functional/hash.hpp>

#include "calc.h"
#include "timepoint.h"



template<class t_timept, class t_real = double>
requires std::floating_point<t_real>
struct TrackPoint
{
	t_real latitude{};               // [deg]
	t_real longitude{};              // [deg]
	t_real elevation{};              // [m]

	t_timept timept{};

	t_real elapsed{};                // time elapsed since previous point  [s]
	t_real elapsed_total{};          // time elapsed since first point [s]

	t_real distance_planar{};        // planar distance to previous point  [m]
	t_real distance_planar_total{};  // planar distance to first point [m]

	t_real distance{};               // full distance to previous point  [m]
	t_real distance_total{};         // full distance to first point [m]
};



/**
 * represents a single running track
 */
template<class t_real = double, class t_size = std::size_t>
requires std::floating_point<t_real> && std::integral<t_size>
class SingleTrack
{
public:
	using t_clk = std::chrono::system_clock;
	using t_time_ty = typename t_clk::rep;
	using t_timept = typename t_clk::time_point;
	using t_dur = typename t_clk::duration;
	using t_sec = std::chrono::duration<t_real, std::ratio<1, 1>>;
	using t_trackpt = TrackPoint<t_timept, t_real>;
	using t_char = typename std::string::value_type;



public:
	SingleTrack() = default;
	~SingleTrack() = default;



	/**
	 * get the function used for distance calculations
	 */
	std::tuple<t_real, t_real>
	(*GetDistanceFunction() const) (
		t_real lat1, t_real lat2,
		t_real lon1, t_real lon2,
		t_real elev1, t_real elev2)

	{
		// default distance function
		std::tuple<t_real, t_real> (*dist_func)(t_real lat1, t_real lat2,
			t_real lon1, t_real lon2,
			t_real elev1, t_real elev2) = &geo_dist<t_real>;

		//std::cout << "distance function: " << m_distance_function << std::endl;
		switch(m_distance_function)
		{
			case 0: dist_func = &geo_dist<t_real>; break;
			case 1: dist_func = &geo_dist_2<t_real, 1>; break;
			case 2: dist_func = &geo_dist_2<t_real, 2>; break;
			case 3: dist_func = &geo_dist_2<t_real, 3>; break;
		}

		return dist_func;
	}



	/**
	 * calculate track properties
	 */
	void Calculate()
	{
		// clear old values
		m_total_dist = 0.;
		m_total_dist_planar = 0.;
		m_total_time = 0.;
		m_ascent = 0.;
		m_descent = 0.;

		// reset ranges
		m_min_elev = std::numeric_limits<t_real>::max();
		m_max_elev = -m_min_elev;
		m_min_lat = std::numeric_limits<t_real>::max();
		m_max_lat = -m_min_lat;
		m_min_long = std::numeric_limits<t_real>::max();
		m_max_long = -m_min_long;

		std::optional<t_real> latitude_last, longitude_last, elevation_last;
		std::optional<t_timept> time_pt_last;

		// distance function
		std::tuple<t_real, t_real> (*dist_func)(t_real lat1, t_real lat2,
			t_real lon1, t_real lon2,
			t_real elev1, t_real elev2) = GetDistanceFunction();

		std::vector<t_real> elevations;
		elevations.reserve(m_points.size());

		for(t_trackpt& trackpt : m_points)
		{
			elevations.push_back(trackpt.elevation);

			// elapsed seconds since last track point
			if(time_pt_last)
				trackpt.elapsed = t_sec{trackpt.timept - *time_pt_last}.count();

			if(latitude_last && longitude_last && elevation_last)
			{
				std::tie(trackpt.distance_planar, trackpt.distance)
					= (*dist_func)(
					*latitude_last, trackpt.latitude,
					*longitude_last, trackpt.longitude,
					*elevation_last, trackpt.elevation);
			}

			// cumulative values
			m_total_time += trackpt.elapsed;
			m_total_dist += trackpt.distance;
			m_total_dist_planar += trackpt.distance_planar;

			// ranges
			m_max_lat = std::max(m_max_lat, trackpt.latitude);
			m_min_lat = std::min(m_min_lat, trackpt.latitude);
			m_max_long = std::max(m_max_long, trackpt.longitude);
			m_min_long = std::min(m_min_long, trackpt.longitude);
			m_max_elev = std::max(m_max_elev, trackpt.elevation);
			m_min_elev = std::min(m_min_elev, trackpt.elevation);

			trackpt.elapsed_total = m_total_time;
			trackpt.distance_total = m_total_dist;
			trackpt.distance_planar_total = m_total_dist_planar;

			// save last values
			latitude_last = trackpt.latitude;
			longitude_last = trackpt.longitude;
			elevation_last = trackpt.elevation;
			time_pt_last = trackpt.timept;
		}  // loop over track points

		if(m_smooth_rad > 0)
			elevations = smooth_data(elevations, static_cast<int>(m_smooth_rad));

		// calulate ascent & descent
		std::optional<t_real> elevation_last_asc;
		for(t_real elevation : elevations)
		{
			// ascent and descent
			if(elevation_last_asc)
			{
				t_real elev_diff = elevation - *elevation_last_asc;
				if(elev_diff > m_asc_eps)
				{
					m_ascent += elev_diff;
					elevation_last_asc = elevation;
				}
				else if(elev_diff < -m_asc_eps)
				{
					m_descent += -elev_diff;
					elevation_last_asc = elevation;
				}
			}

			if(!elevation_last_asc)  // only assign at first point
				elevation_last_asc = elevation;
		}  // loop over elevations
	}



	/**
	 * bin the track time per distance
	 */
	template<template<class...> class t_vec = std::vector>
	std::pair<t_vec<t_real>, t_vec<t_real>>
	GetTimePerDistance(t_real dist_bin = 1000., bool planar = false) const
	{
		t_vec<t_real> times, dists;
		t_size num_bins = static_cast<t_size>(std::ceil(GetTotalDistance(planar) / dist_bin));
		times.reserve(num_bins);
		dists.reserve(num_bins);

		t_real time = 0., dist = 0.;
		t_size bin_idx = 0;

		for(const t_trackpt& pt : m_points)
		{
			time += pt.elapsed;
			dist += planar ? pt.distance_planar : pt.distance;

			while(dist >= dist_bin)
			{
				t_real time_part = time * dist_bin / dist;

				times.push_back(time_part);
				dists.push_back(dist_bin * static_cast<t_real>(bin_idx + 1));

				dist -= dist_bin;
				time -= time_part;
				++bin_idx;
			}
		}

		// add remaining time
		if(time > 0. && dist > 0.)
		{
			times.push_back(time * dist_bin / dist);
			dists.push_back(dist_bin * static_cast<t_real>(bin_idx + 1));
		}

		return std::make_pair(times, dists);
	}



	/**
	 * import a track from a gpx file
	 * @see https://en.wikipedia.org/wiki/GPS_Exchange_Format
	 * @see https://www.topografix.com/gpx/1/1/
	 */
	bool Import(const std::string& trackfilename, t_real assume_dt = 1.)
	{
		namespace ptree = boost::property_tree;
		namespace num = std::numbers;
		namespace fs = __gpx_fs;

		fs::path trackfile{trackfilename};
		if(!fs::exists(trackfile))
			return false;

		ptree::ptree track;
		ptree::read_xml(trackfilename, track);

		const auto& gpx = track.get_child_optional("gpx");
		if(!gpx)
			return false;

		const auto& tracks = gpx->get_child_optional("");
		if(!tracks)
			return false;

		m_filename = trackfile.filename().string();
		m_version = gpx->get<std::string>("<xmlattr>.version", "<unknown>");
		m_creator = gpx->get<std::string>("<xmlattr>.creator", "<unknown>");

		// clear old track points
		m_points.clear();
		t_size pt_idx = 0;

		for(const auto& track : *tracks)
		{
			if(track.first != "trk")
				continue;

			const auto& segs = track.second.get_child_optional("");
			if(!segs)
				continue;

			for(const auto& seg : *segs)
			{
				if(seg.first != "trkseg")
					continue;

				const auto& pts = seg.second.get_child_optional("");
				if(!pts)
					continue;

				for(const auto& pt : *pts)
				{
					if(pt.first != "trkpt")
						continue;

					t_trackpt trackpt
					{
						.latitude = pt.second.get<t_real>("<xmlattr>.lat") / t_real(180) * num::pi_v<t_real>,
						.longitude = pt.second.get<t_real>("<xmlattr>.lon") / t_real(180) * num::pi_v<t_real>,
						.elevation = pt.second.get<t_real>("ele", t_real(0)),
					};

					if(auto time_opt = pt.second.get_optional<std::string>("time"))
					{
						trackpt.timept = to_timepoint<t_clk>(*time_opt);
					}
					else
					{
						trackpt.timept = t_timept{};
						trackpt.timept +=
							static_cast<t_time_ty>(t_real(pt_idx * 1000) * assume_dt) *
							t_dur(std::chrono::milliseconds(1));
					}

					++pt_idx;
					m_points.emplace_back(std::move(trackpt));
				}  // point iteration
			}  // segment iteration
		}  // track iteration

		Calculate();
		CalculateHash();

		return true;
	}



	const std::vector<t_trackpt>& GetPoints() const
	{
		return m_points;
	}



	/**
	 * find the track point that is closest to the given coordinates
	 */
	const t_trackpt* GetClosestPoint(t_real lon, t_real lat) const
	{
		if(m_points.size() == 0)
			return nullptr;

		// distance function
		std::tuple<t_real, t_real> (*dist_func)(t_real lat1, t_real lat2,
			t_real lon1, t_real lon2,
			t_real elev1, t_real elev2) = GetDistanceFunction();

		auto iter = std::min_element(m_points.begin(), m_points.end(),
			[dist_func, lon, lat](const t_trackpt& pt1, const t_trackpt& pt2)
		{
			auto [ dist1_pl, dist1 ] = (*dist_func)(pt1.latitude, lat, pt1.longitude, lon, 0., 0.);
			auto [ dist2_pl, dist2 ] = (*dist_func)(pt2.latitude, lat, pt2.longitude, lon, 0., 0.);

			return dist1_pl < dist2_pl;
		});

		if(iter == m_points.end())
			return nullptr;

		return &*iter;
	}



	std::optional<t_timept> GetStartTime() const
	{
		if(m_points.size() == 0)
			return std::nullopt;

		return m_points.begin()->timept;
	}



	std::optional<t_timept> GetEndTime() const
	{
		if(m_points.size() == 0)
			return std::nullopt;

		return m_points.rbegin()->timept;
	}



	const std::string& GetFileName() const
	{
		return m_filename;
	}



	void SetFileName(const std::string& name)
	{
		m_filename = name;
	}



	const std::string& GetVersion() const
	{
		return m_version;
	}



	const std::string& GetCreator() const
	{
		return m_creator;
	}



	const std::string& GetComment() const
	{
		return m_comment;
	}



	void SetComment(const std::string& comment)
	{
		m_comment = comment;
	}




	t_real GetTotalDistance(bool planar = false) const
	{
		return planar ? m_total_dist_planar : m_total_dist;
	}



	t_real GetTotalTime() const
	{
		return m_total_time;
	}



	std::pair<t_real, t_real> GetLatitudeRange() const
	{
		return std::make_pair(m_min_lat, m_max_lat);
	}



	std::pair<t_real, t_real> GetLongitudeRange() const
	{
		return std::make_pair(m_min_long, m_max_long);
	}



	std::pair<t_real, t_real> GetElevationRange() const
	{
		return std::make_pair(m_min_elev, m_max_elev);
	}



	std::pair<t_real, t_real> GetAscentDescent() const
	{
		return std::make_pair(m_ascent, m_descent);
	}



	t_size GetHash() const
	{
		return m_hash;
	}



	void SetDistanceFunction(int dist_func)
	{
		m_distance_function = dist_func;
	}



	/**
	 * minimum height difference [m] before being counted as climb
	 */
	void SetAscentEpsilon(t_real eps)
	{
		m_asc_eps = eps;
	}



	/**
	 * number of neighbouring point to include in data smoothing
	 */
	void SetSmoothRadius(t_size rad)
	{
		m_smooth_rad = rad;
	}



	bool Save(std::ofstream& ofstr) const
	{
		if(!ofstr)
			return false;

		ofstr.write(reinterpret_cast<const char*>(&m_hash), sizeof(m_hash));

		const t_size num_points = GetPoints().size();
		ofstr.write(reinterpret_cast<const char*>(&num_points), sizeof(num_points));

		// write track points
		for(t_size ptidx = 0; ptidx < num_points; ++ptidx)
		{
			const t_trackpt& pt = GetPoints()[ptidx];

			ofstr.write(reinterpret_cast<const char*>(&pt.latitude), sizeof(pt.latitude));
			ofstr.write(reinterpret_cast<const char*>(&pt.longitude), sizeof(pt.longitude));
			ofstr.write(reinterpret_cast<const char*>(&pt.elevation), sizeof(pt.elevation));

			ofstr.write(reinterpret_cast<const char*>(&pt.elapsed), sizeof(pt.elapsed));
			ofstr.write(reinterpret_cast<const char*>(&pt.elapsed_total), sizeof(pt.elapsed_total));

			ofstr.write(reinterpret_cast<const char*>(&pt.distance_planar), sizeof(pt.distance_planar));
			ofstr.write(reinterpret_cast<const char*>(&pt.distance_planar_total), sizeof(pt.distance_planar_total));

			ofstr.write(reinterpret_cast<const char*>(&pt.distance), sizeof(pt.distance));
			ofstr.write(reinterpret_cast<const char*>(&pt.distance_total), sizeof(pt.distance_total));

			t_real secs = std::chrono::duration_cast<t_sec>(
				pt.timept.time_since_epoch()).count();
			ofstr.write(reinterpret_cast<const char*>(&secs), sizeof(secs));
		}

		// write track data
		ofstr.write(reinterpret_cast<const char*>(&m_total_time), sizeof(m_total_time));
		ofstr.write(reinterpret_cast<const char*>(&m_total_dist_planar), sizeof(m_total_dist_planar));
		ofstr.write(reinterpret_cast<const char*>(&m_total_dist), sizeof(m_total_dist));

		ofstr.write(reinterpret_cast<const char*>(&m_min_lat), sizeof(m_min_lat));
		ofstr.write(reinterpret_cast<const char*>(&m_max_lat), sizeof(m_max_lat));
		ofstr.write(reinterpret_cast<const char*>(&m_min_long), sizeof(m_min_long));
		ofstr.write(reinterpret_cast<const char*>(&m_max_long), sizeof(m_max_long));
		ofstr.write(reinterpret_cast<const char*>(&m_min_elev), sizeof(m_min_elev));
		ofstr.write(reinterpret_cast<const char*>(&m_max_elev), sizeof(m_max_elev));

		ofstr.write(reinterpret_cast<const char*>(&m_ascent), sizeof(m_ascent));
		ofstr.write(reinterpret_cast<const char*>(&m_descent), sizeof(m_descent));

		// write track name
		const t_size name_len = m_filename.size();
		ofstr.write(reinterpret_cast<const char*>(&name_len), sizeof(name_len));
		ofstr.write(reinterpret_cast<const char*>(m_filename.data()), m_filename.size() * sizeof(t_char));

		// write comment
		const t_size comment_len = m_comment.size();
		ofstr.write(reinterpret_cast<const char*>(&comment_len), sizeof(comment_len));
		ofstr.write(reinterpret_cast<const char*>(m_comment.data()), m_comment.size() * sizeof(t_char));

		return true;
	}



	bool Load(std::ifstream& ifstr, bool recalculate = false)
	{
		if(!ifstr)
			return false;

		ifstr.read(reinterpret_cast<char*>(&m_hash), sizeof(m_hash));

		t_size num_points = 0;
		ifstr.read(reinterpret_cast<char*>(&num_points), sizeof(num_points));
		m_points.reserve(num_points);

		// read track points
		for(t_size ptidx = 0; ptidx < num_points; ++ptidx)
		{
			t_trackpt pt{};

			ifstr.read(reinterpret_cast<char*>(&pt.latitude), sizeof(pt.latitude));
			ifstr.read(reinterpret_cast<char*>(&pt.longitude), sizeof(pt.longitude));
			ifstr.read(reinterpret_cast<char*>(&pt.elevation), sizeof(pt.elevation));

			ifstr.read(reinterpret_cast<char*>(&pt.elapsed), sizeof(pt.elapsed));
			ifstr.read(reinterpret_cast<char*>(&pt.elapsed_total), sizeof(pt.elapsed_total));

			ifstr.read(reinterpret_cast<char*>(&pt.distance_planar), sizeof(pt.distance_planar));
			ifstr.read(reinterpret_cast<char*>(&pt.distance_planar_total), sizeof(pt.distance_planar_total));

			ifstr.read(reinterpret_cast<char*>(&pt.distance), sizeof(pt.distance));
			ifstr.read(reinterpret_cast<char*>(&pt.distance_total), sizeof(pt.distance_total));

			t_real secs{};
			ifstr.read(reinterpret_cast<char*>(&secs), sizeof(secs));
			pt.timept = t_timept{static_cast<t_time_ty>(secs * 1000.)
				* std::chrono::milliseconds{1}};
			// pt.timept += std::chrono::hours(1);

			m_points.emplace_back(std::move(pt));
		}

		// read track data
		ifstr.read(reinterpret_cast<char*>(&m_total_time), sizeof(m_total_time));
		ifstr.read(reinterpret_cast<char*>(&m_total_dist_planar), sizeof(m_total_dist_planar));
		ifstr.read(reinterpret_cast<char*>(&m_total_dist), sizeof(m_total_dist));

		ifstr.read(reinterpret_cast<char*>(&m_min_lat), sizeof(m_min_lat));
		ifstr.read(reinterpret_cast<char*>(&m_max_lat), sizeof(m_max_lat));
		ifstr.read(reinterpret_cast<char*>(&m_min_long), sizeof(m_min_long));
		ifstr.read(reinterpret_cast<char*>(&m_max_long), sizeof(m_max_long));
		ifstr.read(reinterpret_cast<char*>(&m_min_elev), sizeof(m_min_elev));
		ifstr.read(reinterpret_cast<char*>(&m_max_elev), sizeof(m_max_elev));

		ifstr.read(reinterpret_cast<char*>(&m_ascent), sizeof(m_ascent));
		ifstr.read(reinterpret_cast<char*>(&m_descent), sizeof(m_descent));

		// read track name
		t_size name_len{};
		ifstr.read(reinterpret_cast<char*>(&name_len), sizeof(name_len));
		m_filename.resize(name_len);
		ifstr.read(reinterpret_cast<char*>(m_filename.data()), m_filename.size() * sizeof(t_char));

		// read comment
		t_size comment_len{};
		ifstr.read(reinterpret_cast<char*>(&comment_len), sizeof(comment_len));
		m_comment.resize(comment_len);
		ifstr.read(reinterpret_cast<char*>(m_comment.data()), m_comment.size() * sizeof(t_char));

		if(recalculate)
		{
			Calculate();
			CalculateHash();
		}

		return true;
	}



	std::string PrintHtml(int prec = -1, bool show_icons = true) const
	{
		std::ostringstream ostr;
		if(prec >= 0)
			ostr.precision(prec);

		t_real t = GetTotalTime();
		t_real s = GetTotalDistance(false);
		t_real s_planar = GetTotalDistance(true);
		auto [ min_elev, max_elev ] = GetElevationRange();
		auto [ asc, desc ] = GetAscentDescent();
		std::optional<t_timept> start_time = GetStartTime();
		std::optional<t_timept> end_time = GetEndTime();

		ostr << "<html>";
		if(show_icons)
			ostr << "<ul style=\"list-style-type: none; margin-left: -32px;\">";
		else
			ostr << "<ul>";

		ostr << "<li>";
		if(show_icons)
			ostr << "&#x1f6f0; ";
		ostr << "<b>Number of track points</b>: " << GetPoints().size() << ".</li>";

		if(start_time && end_time)
		{
			ostr << "<li>";
			if(show_icons)
				ostr << "&#x23f0; ";
			ostr << "<b>Time</b>: "
				<< from_timepoint<t_clk, t_timept>(*start_time, true) << " - "
				<< from_timepoint<t_clk, t_timept>(*end_time, false)
				<< " (" << get_time_str(t) << ").</li>";
		}
		ostr << "<li>";
		if(show_icons)
			ostr << "&#x26f0; ";
		ostr << "<b>Altitudes</b>: [ " << min_elev << ", " << max_elev << " ] m"
			<< " (height difference: " << max_elev - min_elev << " m).</li>";

		ostr << "<li>";
		if(show_icons)
			ostr << "&#x26f0; ";
		ostr << "<b>Climb</b>: " << asc << " m, <b>down</b>: " << desc << " m.</li>";

		ostr << "<li>";
		if(show_icons)
			ostr << "&#x1f4cf; ";
		ostr << "<b>Distance</b>: " << s / 1000. << " km"
			<< " (planar: " << s_planar / 1000. << " km).</li>";

		ostr << "<li>";
		if(show_icons)
			ostr << "&#x1f3c3; ";
		ostr << "<b>Pace</b>: " << get_pace_str((t / 60.) / (s / 1000.))
			<< " (planar: " << get_pace_str((t / 60.) / (s_planar / 1000.)) << ").</li>";

		ostr << "<li>";
		if(show_icons)
			ostr << "&#x1f3c3; ";
		ostr << "<b>Speed</b>: " << (s / 1000.) / (t / 60. / 60.) << " km/h"
			<< " = " << s / t << " m/s" << " (planar: "
			<< (s_planar / 1000.) / (t / 60. / 60.) << " km/h"
			<< " = " << s_planar / t << " m/s" << ").</li>";

		ostr << "</ul>";
		ostr << "</html>";

		return ostr.str();
	}



	friend std::ostream& operator<<(std::ostream& ostr, const SingleTrack<t_real>& track)
	{
		namespace num = std::numbers;

		const int field_width = ostr.precision() + 2;

		// header
		ostr
			<< std::left << std::setw(field_width) << "Lat." << " "
			<< std::left << std::setw(field_width) << "Lon." << " "
			<< std::left << std::setw(field_width) << "h" << " "
			<< std::left << std::setw(field_width) << "\xce\x94t" << "  "
			<< std::left << std::setw(field_width) << "\xce\x94s" << "  "
			<< std::left << std::setw(field_width) << "t" << " "
			<< std::left << std::setw(field_width) << "s" << "\n";

		for(const t_trackpt& pt : track.GetPoints())
		{
			t_real latitude_deg = pt.latitude * t_real(180) / num::pi_v<t_real>;
			t_real longitude_deg = pt.longitude * t_real(180) / num::pi_v<t_real>;

			ostr
				<< std::left << std::setw(field_width) << latitude_deg << " "
				<< std::left << std::setw(field_width) << longitude_deg << " "
				<< std::left << std::setw(field_width) << pt.elevation << " "
				<< std::left << std::setw(field_width) << pt.elapsed << " "
				<< std::left << std::setw(field_width) << pt.distance << " "
				<< std::left << std::setw(field_width) << pt.elapsed_total << " "
				<< std::left << std::setw(field_width) << pt.distance_total << " ";

			// time point
			std::string timestr = from_timepoint<t_clk, t_timept>(pt.timept);
			ostr << std::left << std::setw(25) << timestr << " ";

			ostr << "\n";
		}

		// totals
		t_real t = track.GetTotalTime();
		t_real s = track.GetTotalDistance(false);
		t_real s_planar = track.GetTotalDistance(true);
		auto [ min_elev, max_elev ] = track.GetElevationRange();
		auto [ asc, desc ] = track.GetAscentDescent();

		ostr << "\n";
		ostr << "Number of track points: " << track.GetPoints().size() << "\n";
		ostr << "Altitude range: [ " << min_elev << ", " << max_elev << " ] m\n";
		ostr << "Height difference: " << max_elev - min_elev << " m\n";
		ostr << "Climb: " << asc << " m, descent: " << desc << " m\n";
		ostr << "Total distance: " << s << " m = " << s / 1000. << " km\n";
		ostr << "Total planar distance: " << s_planar / 1000. << " km\n";
		ostr << "Total time: " << get_time_str(t) << "\n";
		ostr << "Speed: " << s / t << " m/s" << " = " << (s / 1000.) / (t / 60. / 60.) << " km/h\n";
		ostr << "Planar speed: " << s_planar / t << " m/s" << " = " << (s_planar / 1000.) / (t / 60. / 60.) << " km/h\n";
		ostr << "Pace: " << get_pace_str((t / 60.) / (s / 1000.)) << "\n";
		ostr << "Planar pace: " << get_pace_str((t / 60.) / (s_planar / 1000.)) << "\n";

		return ostr;
	}



protected:
	void CalculateHash()
	{
		m_hash = 0;

		for(const t_trackpt& pt : m_points)
		{
			t_size lat_hash = std::hash<t_real>{}(pt.latitude);
			t_size lon_hash = std::hash<t_real>{}(pt.longitude);
			t_size ele_hash = std::hash<t_real>{}(pt.elevation);
			t_size time_hash = std::hash<t_size>{}(pt.timept.time_since_epoch().count());

			boost::hash_combine(m_hash, lat_hash);
			boost::hash_combine(m_hash, lon_hash);
			boost::hash_combine(m_hash, ele_hash);
			boost::hash_combine(m_hash, time_hash);
		}
	}



private:
	std::vector<t_trackpt> m_points{};

	std::string m_filename{};
	std::string m_version{}, m_creator{};
	std::string m_comment{};

	t_real m_total_time{};
	t_real m_total_dist_planar{};
	t_real m_total_dist{};

	t_real m_min_lat{}, m_max_lat{};
	t_real m_min_long{}, m_max_long{};
	t_real m_min_elev{}, m_max_elev{};

	// minimum height difference [m] before being counted as climb
	t_real m_asc_eps{5.};
	t_size m_smooth_rad{10};
	t_real m_ascent{}, m_descent{};

	int m_distance_function{0};

	t_size m_hash{};
};


#endif
