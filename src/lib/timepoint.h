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
#include <tuple>
#include <concepts>

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



/**
 * rounds a time point to months
 */
template<class t_clk, class t_timept = typename t_clk::time_point>
t_timept round_timepoint(const t_timept& time_pt, bool yearly = false)
{
	// convert time point to std::tm
	std::tm t{};
	std::time_t tt = t_clk::to_time_t(time_pt);
	boost::date_time::c_time::localtime(&tt, &t);

	// round
	t.tm_mday = 1;
	t.tm_hour = 0;
	t.tm_min = 0;
	t.tm_sec = 0;
	if(yearly)
		t.tm_mon = 0;

	// convert std::tm to time point
	// mktime converts from local time, timegm from UTC
	return t_clk::from_time_t(/*std::mktime*/timegm(&t));
}



/**
 * extract the date from a time point
 */
template<class t_clk, class t_timept = typename t_clk::time_point>
std::tuple<int, int, int> date_from_timept(const t_timept& time_pt)
{
	std::tm t{};
	std::time_t tt = t_clk::to_time_t(time_pt);
	boost::date_time::c_time::localtime(&tt, &t);

	return std::make_tuple(t.tm_year + 1900, t.tm_mon + 1, t.tm_mday);
}



/**
 * extract the date from an epoch
 */
template<class t_clk, class t_timept = typename t_clk::time_point, class t_epoch = double>
std::tuple<int, int, int> date_from_epoch(t_epoch epoch)
{
	t_timept time_pt = t_timept{static_cast<typename t_clk::rep>(
		epoch) * std::chrono::seconds{1}};

	return date_from_timept<t_clk, t_timept>(time_pt);
}



/**
 * extract the date and time from a time point
 */
template<class t_clk, class t_timept = typename t_clk::time_point>
std::tuple<int, int, int, int, int, int> date_time_from_timept(const t_timept& time_pt)
{
	std::tm t{};
	std::time_t tt = t_clk::to_time_t(time_pt);
	boost::date_time::c_time::localtime(&tt, &t);

	return std::make_tuple(t.tm_year + 1900, t.tm_mon + 1, t.tm_mday,
		t.tm_hour, t.tm_min, t.tm_sec);
}



/**
 * extract the date and time from an epoch
 */
template<class t_clk, class t_timept = typename t_clk::time_point, class t_epoch = double>
std::tuple<int, int, int, int, int, int> date_time_from_epoch(t_epoch epoch)
{
	t_timept time_pt = t_timept{static_cast<typename t_clk::rep>(
		epoch) * std::chrono::seconds{1}};

	return date_time_from_timept<t_clk, t_timept>(time_pt);
}



/**
 * get the fraction the current year has progressed
 */
template<class t_clk, class t_real = double, class t_timept = typename t_clk::time_point>
std::tuple<int, t_real> cur_year_progress()
{
	using t_sec = std::chrono::duration<t_real, std::ratio<1, 1>>;

	// now
	t_timept now = t_clk::now();
	std::time_t tt_now = t_clk::to_time_t(now);
	std::tm t_now{};
	boost::date_time::c_time::localtime(&tt_now, &t_now);

	// beginning of current year
	std::tm t_beg = t_now;
	t_beg.tm_mon = 0;
	t_beg.tm_mday = 1;
	t_beg.tm_hour = 0;
	t_beg.tm_min = 0;
	t_beg.tm_sec = 0;
	t_timept beg = t_clk::from_time_t(/*std::mktime*/timegm(&t_beg));

	// beginning of next year
	std::tm t_end = t_now;
	++t_end.tm_year;
	t_end.tm_mon = 0;
	t_end.tm_mday = 1;
	t_end.tm_hour = 0;
	t_end.tm_min = 0;
	t_end.tm_sec = 0;
	t_timept end = t_clk::from_time_t(/*std::mktime*/timegm(&t_end));

	t_real secs_now = std::chrono::duration_cast<t_sec>(now - beg).count();
	t_real secs_end = std::chrono::duration_cast<t_sec>(end - beg).count();
	t_real progress = secs_now / secs_end;

	return std::make_tuple(t_now.tm_year + 1900, progress);
}



template<class t_real = double, class t_int = int>
std::string get_time_str(t_real secs)
requires std::floating_point<t_real> && std::integral<t_int>
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



template<class t_real = double, class t_int = int>
std::string get_dist_str(t_real m)
requires std::floating_point<t_real> && std::integral<t_int>
{
	t_int km = std::floor(m / 1000.);
	m = std::fmod(m, 1000.);

	std::ostringstream ostr;
	bool started = false;
	if(km || started)
	{
		ostr << km << " km";
		started = true;
	}
	if(started)
		ostr << ", ";
	if(m || started)
	{
		ostr << m << " m";
		started = true;
	}

	return ostr.str();
}



template<class t_real = double, class t_int = int>
std::string get_pace_str(t_real min)
requires std::floating_point<t_real> && std::integral<t_int>
{
	t_int secs = std::round(std::fmod(min*60., 60.));
	t_int mins = std::floor(min);

	std::ostringstream ostr;

	ostr << mins << ":" << std::setw(2) << std::setfill('0') << secs;
	ostr << " min/km";
	return ostr.str();
}


#endif
