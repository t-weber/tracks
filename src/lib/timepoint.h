/**
 * track files time point handling
 * @author Tobias Weber (orcid: 0000-0002-7230-1932)
 * @date 24 November 2024
 * @license see 'LICENSE' file
 */

#ifndef __TRACK_TIME_H__
#define __TRACK_TIME_H__

#include <sstream>
#include <iomanip>
#include <chrono>

#include <boost/date_time/c_time.hpp>


#define HAS_CHRONO_PARSE    0
#define HAS_CHRONO_PUTTIME  1



/**
 * converts an UTC time string to a time point
 */
template<class t_clk, class t_timept = typename t_clk::time_point>
t_timept to_timepoint(const std::string& time_str)
{
	t_timept time_pt{};

#if HAS_CHRONO_PARSE != 0
	std::istringstream{time_str} >>
		std::chrono::parse("%4Y-%2m-%2dT%2H:%2M:%2SZ", time_pt);
#else
	std::tm t{};
	t.tm_year = std::stoi(time_str.substr(0, 4)) - 1900;
	t.tm_mon = std::stoi(time_str.substr(5, 2)) - 1;
	t.tm_mday = std::stoi(time_str.substr(8, 2));
	t.tm_hour = std::stoi(time_str.substr(11, 2));
	t.tm_min = std::stoi(time_str.substr(14, 2));
	t.tm_sec = std::stoi(time_str.substr(17, 2));
	t.tm_isdst = -1;

	// mktime converts from local time, timegm from UTC
	time_pt = t_clk::from_time_t(/*std::mktime*/timegm(&t));
#endif

	return time_pt;
}



/**
 * converts a time point to a local time string
 */
template<class t_clk, class t_timept = typename t_clk::time_point>
std::string from_timepoint(const t_timept& time_pt,
	bool show_date = true, bool show_time = true)
{
	std::tm t{};
	std::time_t tt = t_clk::to_time_t(time_pt);
	boost::date_time::c_time::localtime(&tt, &t);

	std::ostringstream ostr;

#if HAS_CHRONO_PUTTIME != 0
	if(show_date)
		ostr << std::put_time(&t, "%Y-%m-%d");
	if(show_date && show_time)
		ostr << " ";
	if(show_time)
		ostr << std::put_time(&t, "%H:%M:%S");
#else
	if(show_date)
	{
		ostr
			<< std::setw(4) << std::setfill('0') << (t.tm_year + 1900) << "-"
			<< std::setw(2) << std::setfill('0') << (t.tm_mon + 1) << "-"
			<< std::setw(2) << std::setfill('0') << t.tm_mday
	}

	if(show_date && show_time)
		ostr << " ";

	if(show_time)
	{
		ostr
			<< std::setw(2) << std::setfill('0') << t.tm_hour << ":"
			<< std::setw(2) << std::setfill('0') << t.tm_min << ":"
			<< std::setw(2) << std::setfill('0') << t.tm_sec;
	}
#endif

	return ostr.str();
}



template<class t_real = double, class t_int = int>
std::string get_time_str(t_real secs)
{
	t_int h = std::floor(secs / 60. / 60.);
	secs = std::fmod(secs, 60. * 60.);

	t_int m = std::floor(secs / 60.);
	secs = std::fmod(secs, 60.);

	std::ostringstream ostr;
	bool started = false;
	if(h || started)
	{
		ostr << h << " h";
		started = true;
	}
	if(started)
		ostr << ", ";
	if(m || started)
	{
		ostr << m << " min";
		started = true;
	}
	if(started)
		ostr << ", ";
	if(secs || started)
	{
		ostr << secs << " s";
		started = true;
	}

	return ostr.str();
}


#endif
