/**
 * basic calculations
 * @author Tobias Weber (orcid: 0000-0002-7230-1932)
 * @date 24 November 2024
 * @license see 'LICENSE' file
 */

#ifndef __TRACK_CALC_H__
#define __TRACK_CALC_H__

#include <tuple>
#include <cmath>
#include <concepts>
#include <type_traits>

#include <boost/geometry.hpp>



/**
 * haversine
 * @see https://en.wikipedia.org/wiki/Versine#Haversine
 */
template<typename t_real = double>
t_real havsin(t_real th)
requires std::floating_point<t_real>
{
	return t_real(0.5) - t_real(0.5)*std::cos(th);
}



/**
 * arcaversin
 * @see https://en.wikipedia.org/wiki/Versine#Haversine
 */
template<typename t_real = double>
t_real arcaversin(t_real x)
requires std::floating_point<t_real>
{
	return std::acos(t_real(1) - t_real(2)*x);
}



/**
 * earth radius
 * @see https://en.wikipedia.org/wiki/Earth_radius
 */
template<typename t_real = double>
t_real earth_radius(t_real lat)
requires std::floating_point<t_real>
{
	t_real rad_pol = 6.3567523e6;
	t_real rad_equ = 6.3781370e6;

	t_real c = std::cos(lat);
	t_real s = std::sin(lat);

	t_real num =
		std::pow(rad_equ*rad_equ*c, t_real(2)) +
		std::pow(rad_pol*rad_pol*s, t_real(2));
	t_real den =
		std::pow(rad_equ*c, t_real(2)) +
		std::pow(rad_pol*s, t_real(2));

	return std::sqrt(num / den);
}



/**
 * haversine formula
 * @see https://en.wikipedia.org/wiki/Haversine_formula
 */
template<typename t_real = double>
std::tuple<t_real, t_real>  // [ planar distance, distance including heights ]
geo_dist(t_real lat1, t_real lat2,
	t_real lon1, t_real lon2,
	t_real elev1, t_real elev2)
requires std::floating_point<t_real>
{
	t_real rad = earth_radius<t_real>((lat1 + lat2) / t_real(2));
	rad += (elev1 + elev2) * t_real(0.5);

	t_real h =
		havsin<t_real>(lat2 - lat1) +
		havsin<t_real>(lon2 - lon1) * std::cos(lat1)*std::cos(lat2);

	t_real dist = rad * arcaversin<t_real>(h);
	t_real elev_diff = elev2 - elev1;
	return std::make_tuple(dist, std::sqrt(dist*dist + elev_diff*elev_diff));
}



/**
 * geographic distance
 * @see https://github.com/BoostGSoC18/geometry/blob/develop/test/strategies/thomas.cpp
 * @see https://github.com/boostorg/geometry/blob/develop/example/ml02_distance_strategy.cpp
 */
template<typename t_real = double, int STRATEGY = 0>
std::tuple<t_real, t_real>  // [ planar distance, distance including heights ]
geo_dist_2(t_real lat1, t_real lat2,
	t_real lon1, t_real lon2,
	[[__maybe_unused__]] t_real elev1, [[__maybe_unused__]] t_real elev2)
requires std::floating_point<t_real>
{
	namespace geo = boost::geometry;
	using t_pt = geo::model::point<t_real, 2, geo::cs::geographic<geo::radian>>;

	t_pt pt1{lon1, lat1};
	t_pt pt2{lon2, lat2};

	t_real dist = t_real(0);

	if constexpr(STRATEGY == 1)
	{
		using t_strat = geo::strategy::distance::thomas<geo::srs::spheroid<t_real>>;
		dist = geo::distance<t_pt, t_pt, t_strat>(pt1, pt2, t_strat{});
	}
	else if constexpr(STRATEGY == 2)
	{
		using t_strat = geo::strategy::distance::vincenty<geo::srs::spheroid<t_real>>;
		dist = geo::distance<t_pt, t_pt, t_strat>(pt1, pt2, t_strat{});
	}
#if BOOST_VERSION > 107400
	else if constexpr(STRATEGY == 3)
	{
		using t_strat = geo::strategy::distance::karney<geo::srs::spheroid<t_real>>;
		dist = geo::distance<t_pt, t_pt, t_strat>(pt1, pt2, t_strat{});
	}
#endif
	else
	{
		t_real rad = earth_radius<t_real>((lat1 + lat2) / t_real(2));
		rad += (elev1 + elev2) * t_real(0.5);

		using t_strat = geo::strategy::distance::haversine<geo::srs::spheroid<t_real>>;
		dist = geo::distance<t_pt, t_pt, t_strat>(pt1, pt2, t_strat{rad});
	}

	t_real elev_diff = elev2 - elev1;
	return std::make_tuple(dist, std::sqrt(dist*dist + elev_diff*elev_diff));
}



/**
 * km/h <-> min/km
 */
template<typename t_real = double>
t_real speed_to_pace(t_real speed)
requires std::floating_point<t_real>
{
	return t_real(60.) / speed;
}



/**
 * smoothing of data points
 * see: https://en.wikipedia.org/wiki/Laplacian_smoothing
 */
template<class t_cont>
t_cont smooth_data(const t_cont& vec, int N = 1)
requires requires(t_cont cont)
{
	typename t_cont::value_type;
	cont.size();
	cont[0] = cont[1];
}
{
	if(N <= 0)
		return vec;

	using t_val = typename t_cont::value_type;
	const int SIZE = static_cast<int>(vec.size());

	t_cont smoothed = vec;

	for(int i = 0; i < SIZE; ++i)
	{
		t_val elem{};
		t_val num{};

		for(int j = -N; j <= N; ++j)
		{
			int idx = i + j;
			if(idx >= SIZE || idx < 0)
				continue;

			elem += vec[i + j];
			num += 1;
		}

		smoothed[i] = elem / num;
	}

	return smoothed;
}


#endif
