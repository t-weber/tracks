/**
 * gpx track file loader
 * @author Tobias Weber
 * @date 24 November 2024
 * @license see 'LICENSE' file
 */

#ifndef __TRACK_GPX_H__
#define __TRACK_GPX_H__

#include <iostream>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <limits>
#include <cmath>
#include <numbers>

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>

#include "calc.h"


#define HAS_CHRONO_PARSE 0



template<class t_clk, class t_timept = typename t_clk::time_point>
t_timept get_timepoint(const std::string& time_str)
{
	t_timept time_pt{};

#if HAS_CHRONO_PARSE != 0
	std::istringstream{time_str} >>
		std::chrono::parse("%4Y-%2m-%2dT%2H:%2M:%2SZ", time_pt);
#else
	std::tm t{};
	t.tm_year = std::stoi(time_str.substr(0, 4));
	t.tm_mon = std::stoi(time_str.substr(5, 2)) + 1;
	t.tm_mday = std::stoi(time_str.substr(8, 2));
	t.tm_hour = std::stoi(time_str.substr(11, 2));
	t.tm_min = std::stoi(time_str.substr(14, 2));
	t.tm_sec = std::stoi(time_str.substr(17, 2));

	time_pt = t_clk::from_time_t(std::mktime(&t));
#endif

	return time_pt;
}



template<class t_timept, class t_real = double>
struct GpxPoint
{
	t_real latitude{};
	t_real longitude{};
	t_real elevation{};

	std::optional<t_timept> timept{};
	std::string timestr{};

	t_real elapsed{};                // time elapsed since last point
	t_real elapsed_total{};          // time elapsed since first point

	t_real distance_planar{};        // planar distance to last point
	t_real distance{};               // full distance to last point

	t_real distance_planar_total{};  // planar distance to first point
	t_real distance_total{};         // full distance to first point
};



/**
 * loads a gpx track file
 * @see https://en.wikipedia.org/wiki/GPS_Exchange_Format
 * @see https://www.topografix.com/gpx/1/1/
 */
template<class t_real = double>
class Gpx
{
public:
	using t_clk = std::chrono::system_clock;
	using t_timept = typename t_clk::time_point;
	using t_GpxPoint = GpxPoint<t_timept, t_real>;


public:
	Gpx() = default;
	~Gpx() = default;


	bool Load(const std::string& trackfile, t_real assume_dt = 1.)
	{
		namespace ptree = boost::property_tree;
		namespace num = std::numbers;

		ptree::ptree track;
		ptree::read_xml(trackfile, track);

		const auto& gpx = track.get_child_optional("gpx");
		if(!gpx)
			return false;

		const auto& tracks = gpx->get_child_optional("");
		if(!tracks)
			return false;

		m_version = gpx->get<std::string>("<xmlattr>.version", "<unknown>");
		m_creator = gpx->get<std::string>("<xmlattr>.creator", "<unknown>");

		// clear old values
		m_points.clear();
		m_total_dist = 0.;
		m_total_dist_planar = 0.;
		m_total_time = 0.;
		m_min_elev = std::numeric_limits<t_real>::max();
		m_max_elev = -m_min_elev;

		std::optional<t_real> latitude_last, longitude_last, elevation_last;
		std::optional<t_timept> time_pt_last;

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

					t_GpxPoint trackpt
					{
						.latitude = pt.second.get<t_real>("<xmlattr>.lat") / t_real(180) * num::pi_v<t_real>,
						.longitude = pt.second.get<t_real>("<xmlattr>.lon") / t_real(180) * num::pi_v<t_real>,
						.elevation = pt.second.get<t_real>("ele", t_real(0)),
					};

					bool has_time = false;
					if(auto time_opt = pt.second.get_optional<std::string>("time"))
					{
						trackpt.timestr = *time_opt;
						trackpt.timept = get_timepoint<t_clk>(trackpt.timestr);
						has_time = true;
					}

					// elapsed seconds since last track point
					if(time_pt_last)
					{
						if(has_time)
							trackpt.elapsed = std::chrono::duration<t_real>{*trackpt.timept - *time_pt_last}.count();
						else
							trackpt.elapsed = assume_dt;
					}

					if(latitude_last && longitude_last && elevation_last)
					{
						std::tie(trackpt.distance_planar, trackpt.distance) = geo_dist/*_2*/<t_real>(
							*latitude_last, trackpt.latitude,
							*longitude_last, trackpt.longitude,
							*elevation_last, trackpt.elevation);
					}

					// cumulative values
					m_total_time += trackpt.elapsed;
					m_total_dist += trackpt.distance;
					m_total_dist_planar += trackpt.distance_planar;
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

					m_points.emplace_back(std::move(trackpt));
				}  // point iteration
			}  // segment iteration
		}  // track iteration

		return true;
	}


	const std::vector<t_GpxPoint>& GetPoints() const
	{
		return m_points;
	}


	const std::string& GetVersion() const
	{
		return m_version;
	}


	const std::string& GetCreator() const
	{
		return m_creator;
	}


	t_real GetTotalDistance(bool planar = false) const
	{
		return planar ? m_total_dist_planar : m_total_dist;
	}


	t_real GetTotalTime() const
	{
		return m_total_time;
	}


	std::pair<t_real, t_real> GetElevationRange() const
	{
		return std::make_pair(m_min_elev, m_max_elev);
	}


	friend std::ostream& operator<<(std::ostream& ostr, const Gpx<t_real>& track)
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

		for(const t_GpxPoint& pt : track.GetPoints())
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

			if(pt.timept)
				ostr << std::left << std::setw(25) << pt.timestr << " ";

			ostr << "\n";
		}

		// totals
		auto [ min_elev, max_elev ] = track.GetElevationRange();
		t_real t = track.GetTotalTime();
		t_real s = track.GetTotalDistance(false);

		ostr << "\n";
		ostr << "Number of track points: " << track.GetPoints().size() << "\n";
		ostr << "Elevation range: [ " << min_elev << ", " << max_elev << " ] m\n";
		ostr << "Height difference: " << max_elev - min_elev << " m\n";
		ostr << "Total distance: " << s / 1000. << " km\n";
		ostr << "Total planar distance: " << track.GetTotalDistance(true) / 1000. << " km\n";
		ostr << "Total time: " << t / 60. << " min\n";
		ostr << "Speed: " << s / t << " m/s" << " = " << (s / 1000.) / (t / 60. / 60.) << " km/h\n";
		ostr << "Pace: " << (t / 60.) / (s / 1000.) << " min/km\n";

		return ostr;
	}


private:
	std::vector<t_GpxPoint> m_points{};
	std::string m_version{}, m_creator{};

	t_real m_total_dist{};
	t_real m_total_dist_planar{};
	t_real m_total_time{};
	t_real m_min_elev{};
	t_real m_max_elev{};
};


#endif
