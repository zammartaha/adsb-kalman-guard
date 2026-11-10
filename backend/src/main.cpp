#include "aircraft_ping.hpp"
#include "enu_projector.hpp"
#include "fleet_manager.hpp"
#include <iostream>

int	main() {

	std::ios::sync_with_stdio(false);	// optimization suggested by AI

	std::cerr << "Reference Antenna Origin: (" << REF_LAT << ", " << REF_LON << ") Alt: " << REF_ALT_M << "m\n"
			  << "--------------------------------------------------------------------------------" << std::endl;

	std::string line;
	aircraft_ping ping;
	fleet_manager fleet;

	while (std::getline(std::cin, line)) {
		if (parse_readsb_line(line, ping)) {
			fleet.process_ping(ping);
		}
	}
	return 0;
}