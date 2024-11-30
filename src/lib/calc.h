/**
 * basic calculations
 * @author Tobias Weber
 * @date 24 November 2024
 * @license see 'LICENSE' file
 */

#ifndef __TRACK_CALC_H__
#define __TRACK_CALC_H__

#include <tuple>
#include <cmath>

#include <boost/geometry.hpp>



/**
 * haversine
 * @see https://en.wikipedia.org/wiki/Versine#Haversine
 */
template<typename t_real = double>
t_real havsin(t_real th)
{
	return t_real(0.5) - t_real(0.5)*std::cos(th);
}



/**
 * arcaversin
 * @see https://en.wikipedia.org/wiki/Versine#Haversine
 */
template<typename t_real = double>
t_real arcaversin(t_real x)
{
	return std::acos(t_real(1) - t_real(2)*x);
}



/**
 * earth radius
 * @see https://en.wikipedia.org/wiki/Earth_radius
 */
template<typename t_real = double>
t_real earth_radius(t_real lat)
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
{
	t_real rad = earth_radius<t_real>((lat1 + lat2) / t_real(2));
	rad += (elev1 + elev2) * t_real(0.5);

	t_real h =
		havsin<t_real>(lat2 - lat1) +
		havsin<t_real>(lon2 - lon1) * std::cos(lat1)*std::cos(lat2);

	t_real dist = rad * arcaversin<t_real>(h);
	return std::make_tuple(dist, dist + std::abs(elev2 - elev1));
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
{
	namespace geo = boost::geometry;
	using t_pt = geo::model::point<t_real, 2, geo::cs::geographic<geo::radian>>;

	t_pt pt1{lon1, lat1};
	t_pt pt2{lon2, lat2};

	t_real dist = t_real(0);

	if constexpr(STRATEGY == 0)
	{
		using t_strat = geo::strategy::distance::thomas<geo::srs::spheroid<t_real>>;
		dist = geo::distance<t_pt, t_pt, t_strat>(pt1, pt2, t_strat{});
	}
	else if constexpr(STRATEGY == 1)
	{
		using t_strat = geo::strategy::distance::vincenty<geo::srs::spheroid<t_real>>;
		dist = geo::distance<t_pt, t_pt, t_strat>(pt1, pt2, t_strat{});
	}
	else
	{
		t_real rad = earth_radius<t_real>((lat1 + lat2) / t_real(2));
		rad += (elev1 + elev2) * t_real(0.5);

		using t_strat = geo::strategy::distance::haversine<geo::srs::spheroid<t_real>>;
		dist = geo::distance<t_pt, t_pt, t_strat>(pt1, pt2, t_strat{rad});
	}

	return std::make_tuple(dist, dist + std::abs(elev2 - elev1));
}


#endif
