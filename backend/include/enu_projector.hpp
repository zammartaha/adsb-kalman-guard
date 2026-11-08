#pragma once
#include "aircraft_ping.hpp"
#include <cmath>
#include <cstdlib>

inline double get_env_double(const char* name, double default_val) {
	const char* val = std::getenv(name);
	if (val != nullptr) {
		return std::stod(val);
		} else {
			return default_val;
		}
}

inline const double REF_LAT		= get_env_double("REF_LAT", 0.0);
inline const double REF_LON		= get_env_double("REF_LON", 0.0);
inline const double REF_ALT_M	= get_env_double("REF_ALT_M", 0.0);
constexpr double EARTH_RADIUS_M	= 6378137.0;
constexpr double DEG_TO_RAD		= M_PI / 180.0;
constexpr double RAD_TO_DEG		= 180.0 / M_PI;

struct	enu_state
{
	double	x;				// East-West distance from antenna (meters, + is East)
	double	y;				// North-South distance from antenna (meters, + is North)
	double	z;				// Up-Down height above antenna (meters, + is Up)
	double	vx;				// East-West velocity (meters/second)
	double	vy;				// North-South velocity (meters/second)
	double	vz;				// Vertical climb/sink velocity (meters/second)
	bool	has_velocity;	// true if the ping had valid speed & track_deg
};

enu_state	ping_to_enu(const aircraft_ping& ping);
void		enu_to_latlon(double x, double y, double z, double& out_lat, double& out_lon, double& out_alt_m);