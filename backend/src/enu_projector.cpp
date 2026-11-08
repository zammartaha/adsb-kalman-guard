#include "enu_projector.hpp"

enu_state	ping_to_enu(const aircraft_ping& ping) {
	double	d_lat_rad = (ping.lat - REF_LAT) * DEG_TO_RAD;
	double	d_lon_rad = (ping.lon - REF_LON) * DEG_TO_RAD;
	double	ref_lat_rad = REF_LAT * DEG_TO_RAD;
	double	track_rad = ping.track_deg * DEG_TO_RAD;

	enu_state state;
	state.x = EARTH_RADIUS_M * d_lon_rad * std::cos(ref_lat_rad);
	state.y = EARTH_RADIUS_M * d_lat_rad;
	state.z = ping.alt_m - REF_ALT_M;
	if (ping.speed >= 0.0 && ping.track_deg >= 0.0) {
		state.has_velocity = true;
		state.vx = ping.speed * std::sin(track_rad);
		state.vy = ping.speed * std::cos(track_rad);
		state.vz = ping.vrate_mps;
	} else {
		state.has_velocity = false;
		state.vx = 0.0;
		state.vy = 0.0;
		state.vz = 0.0;
	}
	return state;
}

void	enu_to_latlon(double x, double y, double z, double& out_lat, double& out_lon, double& out_alt_m) {
	double d_lat_rad = y / EARTH_RADIUS_M;
	out_lat = REF_LAT + (d_lat_rad * RAD_TO_DEG);
	double ref_lat_rad = REF_LAT * DEG_TO_RAD;
	double d_lon_rad = x / (EARTH_RADIUS_M * std::cos(ref_lat_rad));
	out_lon = REF_LON + (d_lon_rad * RAD_TO_DEG);
	out_alt_m = z + REF_ALT_M;
}