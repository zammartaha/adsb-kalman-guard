#include "aircraft_ping.hpp"
#include "enu_projector.hpp"
#include <fstream>
#include <iostream>
#include <iomanip>

int	main(int argc, char** argv) {
	if (argc < 2) {
		std::cerr << "Usage: ./kalman_server <path_to_ndjson_file>\n";
		return 1;
	}

	std::ifstream file(argv[1]);
	if (!file.is_open()) {
		std::cerr << "Failed to open file: " << argv[1] << "\n";
		return 1;
	}

	std::cout << "Reference Antenna Origin: (" << REF_LAT << ", " << REF_LON
			  << ") Alt: " << REF_ALT_M << "m\n"
			  << "--------------------------------------------------------------------------------\n";

	std::string line;
	aircraft_ping ping;
	int count = 0;

	while (std::getline(file, line)) {
		if (parse_readsb_line(line, ping)) {
			count++;
			enu_state enu = ping_to_enu(ping);

			double check_lat = 0.0, check_lon = 0.0, check_alt = 0.0;
			enu_to_latlon(enu.x, enu.y, enu.z, check_lat, check_lon, check_alt);

			std::cout << std::fixed << std::setprecision(2)
					  << "[" << count << "] ICAO: " << ping.hex
					  << " | Call: " << ping.callsign << "\n"
					  << "    ENU Pos (m) : East(x)=" << enu.x << "m, North(y)=" << enu.y << "m, Up(z)=" << enu.z << "m\n"
					  << "    ENU Vel (m/s): vx=" << enu.vx << ", vy=" << enu.vy << ", vz=" << enu.vz
					  << " (has_vel=" << (enu.has_velocity ? "yes" : "no") << ")\n"
					  << std::setprecision(5)
					  << "    Round-Trip  : Orig(" << ping.lat << ", " << ping.lon << ") -> Back("
					  << check_lat << ", " << check_lon << ")\n\n";
		}
	}
	return 0;
}