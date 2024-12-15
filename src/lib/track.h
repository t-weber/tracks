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

#if __has_include(<filesystem>)
	#include <filesystem>
	namespace __gpx_fs = std::filesystem;
#else
	#include <boost/filesystem.hpp>
	namespace __gpx_fs = boost::filesystem;
#endif

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>

#include "calc.h"
#include "timepoint.h"



template<class t_timept, class t_real = double>
struct TrackPoint
{
	t_real latitude{};
	t_real longitude{};
	t_real elevation{};

	t_timept timept{};

	t_real elapsed{};                // time elapsed since last point
	t_real elapsed_total{};          // time elapsed since first point

	t_real distance_planar{};        // planar distance to last point
	t_real distance_planar_total{};  // planar distance to first point

	t_real distance{};               // full distance to last point
	t_real distance_total{};         // full distance to first point
};



/**
 * represents a single running track
 */
template<class t_real = double, class t_size = std::size_t>
class SingleTrack
{
public:
	using t_clk = std::chrono::system_clock;
	using t_time_ty = typename t_clk::rep;
	using t_timept = typename t_clk::time_point;
	using t_dur = typename t_clk::duration;
	using t_sec = std::chrono::duration<t_real, std::ratio<1, 1>>;
	using t_TrackPoint = TrackPoint<t_timept, t_real>;


public:
	SingleTrack() = default;
	~SingleTrack() = default;


	/**
	 * calculate track properties
	 */
	void Calculate()
	{
		// clear old values
		m_total_dist = 0.;
		m_total_dist_planar = 0.;
		m_total_time = 0.;

		// reset ranges
		m_min_elev = std::numeric_limits<t_real>::max();
		m_max_elev = -m_min_elev;
		m_min_lat = std::numeric_limits<t_real>::max();
		m_max_lat = -m_min_lat;
		m_min_long = std::numeric_limits<t_real>::max();
		m_max_long = -m_min_long;

		std::optional<t_real> latitude_last, longitude_last, elevation_last;
		std::optional<t_timept> time_pt_last;

		// get distance function
		std::tuple<t_real, t_real> (*dist_func)(t_real lat1, t_real lat2,
			t_real lon1, t_real lon2,
			t_real elev1, t_real elev2) = &geo_dist<t_real>;

		//std::cout << "distance function: " << m_distance_function << std::endl;
		switch(m_distance_function)
		{
			case 0: dist_func = &geo_dist<t_real>; break;
			case 1: dist_func = &geo_dist_2<t_real, 1>; break;
			case 2: dist_func = &geo_dist_2<t_real, 2>; break;
		}

		for(t_TrackPoint& trackpt : m_points)
		{
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
		}
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

					t_TrackPoint trackpt
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
		return true;
	}


	const std::vector<t_TrackPoint>& GetPoints() const
	{
		return m_points;
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


	bool Save(std::ofstream& ofstr) const
	{
		if(!ofstr)
			return false;

		const t_size num_points = GetPoints().size();
		ofstr.write(reinterpret_cast<const char*>(&num_points), sizeof(num_points));

		// write track points
		for(t_size ptidx = 0; ptidx < num_points; ++ptidx)
		{
			const t_TrackPoint& pt = GetPoints()[ptidx];

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

		// write track name
		const t_size name_len = m_filename.size();
		ofstr.write(reinterpret_cast<const char*>(&name_len), sizeof(name_len));
		ofstr.write(reinterpret_cast<const char*>(m_filename.data()), m_filename.size() * sizeof(char));

		return true;
	}


	bool Load(std::ifstream& ifstr, bool recalculate = false)
	{
		if(!ifstr)
			return false;

		t_size num_points = 0;
		ifstr.read(reinterpret_cast<char*>(&num_points), sizeof(num_points));
		m_points.reserve(num_points);

		// read track points
		for(t_size ptidx = 0; ptidx < num_points; ++ptidx)
		{
			t_TrackPoint pt{};

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

		// read track name
		t_size name_len{};
		ifstr.read(reinterpret_cast<char*>(&name_len), sizeof(name_len));
		m_filename.resize(name_len);
		ifstr.read(reinterpret_cast<char*>(m_filename.data()), m_filename.size() * sizeof(char));

		if(recalculate)
			Calculate();
		return true;
	}


	std::string PrintHtml(int prec = -1) const
	{
		std::ostringstream ostr;
		if(prec >= 0)
			ostr.precision(prec);

		t_real t = GetTotalTime();
		t_real s = GetTotalDistance(false);
		t_real s_planar = GetTotalDistance(true);
		auto [ min_elev, max_elev ] = GetElevationRange();
		std::optional<t_timept> start_time = GetStartTime();
		std::optional<t_timept> end_time = GetEndTime();

		ostr << "<html>";
		ostr << "<ul>";

		ostr << "<li>Number of track points: " << GetPoints().size() << ".</li>";
		if(start_time && end_time)
		{
			ostr << "<li>Track time: "
				<< from_timepoint<t_clk, t_timept>(*start_time, true) << " - "
				<< from_timepoint<t_clk, t_timept>(*end_time, false)
				<< " (" << get_time_str(t) << ").</li>";
		}
		ostr << "<li>Elevation range: [ " << min_elev << ", " << max_elev << " ] m"
			<< " (height difference: " << max_elev - min_elev << " m).</li>";
		ostr << "<li>Distance: " << s / 1000. << " km"
			<< " (planar: " << s_planar / 1000. << " km).</li>";
		ostr << "<li>Pace: " << get_pace_str((t / 60.) / (s / 1000.))
			<< " (planar: " << get_pace_str((t / 60.) / (s_planar / 1000.)) << ").</li>";
		ostr << "<li>Speed: " << s / t << " m/s" << " = " << (s / 1000.) / (t / 60. / 60.) << " km/h"
			<< " (planar: " << s_planar / t << " m/s" << " = " << (s_planar / 1000.) / (t / 60. / 60.) << " km/h).</li>";

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

		for(const t_TrackPoint& pt : track.GetPoints())
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

		ostr << "\n";
		ostr << "Number of track points: " << track.GetPoints().size() << "\n";
		ostr << "Elevation range: [ " << min_elev << ", " << max_elev << " ] m\n";
		ostr << "Height difference: " << max_elev - min_elev << " m\n";
		ostr << "Total distance: " << s << " m = " << s / 1000. << " km\n";
		ostr << "Total planar distance: " << s_planar / 1000. << " km\n";
		ostr << "Total time: " << get_time_str(t) << "\n";
		ostr << "Speed: " << s / t << " m/s" << " = " << (s / 1000.) / (t / 60. / 60.) << " km/h\n";
		ostr << "Planar speed: " << s_planar / t << " m/s" << " = " << (s_planar / 1000.) / (t / 60. / 60.) << " km/h\n";
		ostr << "Pace: " << get_pace_str((t / 60.) / (s / 1000.)) << "\n";
		ostr << "Planar pace: " << get_pace_str((t / 60.) / (s_planar / 1000.)) << "\n";

		return ostr;
	}


	void SetDistanceFunction(int dist_func)
	{
		m_distance_function = dist_func;
	}


private:
	std::vector<t_TrackPoint> m_points{};

	std::string m_filename{};
	std::string m_version{}, m_creator{};

	t_real m_total_time{};
	t_real m_total_dist_planar{};
	t_real m_total_dist{};

	t_real m_min_lat{}, m_max_lat{};
	t_real m_min_long{}, m_max_long{};
	t_real m_min_elev{}, m_max_elev{};

	int m_distance_function{0};
};


#endif
