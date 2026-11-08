#include "aircraft_ping.hpp"
#include "enu_projector.hpp"
#include "kalman_tracker.hpp"
#include <iostream>
#include <iomanip>
#include <unordered_map>
#include <chrono>

std::string get_env_string(const char* name, const std::string& default_val) {
	const char* val = std::getenv(name);
	return (val != nullptr) ? std::string(val) : default_val;
}

void prune_stale_trackers(
	std::unordered_map<std::string, kalman_tracker>& trackers,
	std::unordered_map<std::string, int>& ping_counts,
	double current_time) {
	auto it = trackers.begin();
	while (it != trackers.end()) {
		if (current_time - it->second.timestamp > TRACK_TIMEOUT_SEC) {
			ping_counts.erase(it->first);
			it = trackers.erase(it);
		} else
			++it;
	}
}

int	main() {

	std::cout << "Reference Antenna Origin: (" << REF_LAT << ", " << REF_LON << ") Alt: " << REF_ALT_M << "m\n"
			  << "--------------------------------------------------------------------------------\n";

	std::string line;
	aircraft_ping ping;

	std::unordered_map<std::string, kalman_tracker> trackers;
	std::unordered_map<std::string, int> ping_counts;

	std::string spoof_target = get_env_string("SPOOF_HEX", "NONE");

	auto last_sweep_time = std::chrono::steady_clock::now();

	while (std::getline(std::cin, line)) {
		if (parse_readsb_line(line, ping)) {

			auto now = std::chrono::steady_clock::now();
			double elapsed_time = std::chrono::duration<double>(now - last_sweep_time).count();

			if (elapsed_time >= SWEEP_INTERVAL_SEC) {
				prune_stale_trackers(trackers, ping_counts, ping.timestamp);
				last_sweep_time = now;
			}

			enu_state enu = ping_to_enu(ping);
			ping_counts[ping.hex]++;
			if ((ping.hex == spoof_target || spoof_target == "ANY") && ping_counts[ping.hex] >= 3 && ping_counts[ping.hex] <= 50) {
				std::cout << "\n[⚠️ INJECTING FAKE SPOOF ATTACK ON " << ping.hex << " (+5000m East, -5000m North)]\n";
				enu.x += 5000.0;
				enu.y -= 5000.0;
			}
			if (trackers.find(ping.hex) == trackers.end() ||
				(ping.timestamp - trackers[ping.hex].timestamp) > TRACK_TIMEOUT_SEC) {
				trackers[ping.hex] = kalman_tracker(ping, enu);
				ping_counts[ping.hex] = 1;
			} else
				trackers[ping.hex].update(ping, enu);

			kalman_tracker& t = trackers[ping.hex];

			std::cout << std::fixed << std::setprecision(2)
					  << "[" << ping_counts[ping.hex] << "] ICAO: " << ping.hex
					  << " | Call: " << ping.callsign << "\n"
					  << "    Raw      ENU Pos (m)  : East(x)=" << enu.x << ", North(y)=" << enu.y << ", Up(z)=" << enu.z << "\n"
					  << "    Filtered ENU Pos (m)  : East(x)=" << t.x(0) << ", North(y)=" << t.x(1) << ", Up(z)=" << t.x(2) << "\n"
					  << "    Raw      ENU Vel (m/s): vx=" << enu.vx << ", vy=" << enu.vy << ", vz=" << enu.vz << "\n"
					  << "    Filtered ENU Vel (m/s): vx=" << t.x(3) << ", vy=" << t.x(4) << ", vz=" << t.x(5) << "\n"
					  << "    ping have velocety : " << (enu.has_velocity ? "yes" : "no") << "\n"
					  << "    spoofing score=" << t.last_mahalanobis << ", is spoofed: " << (t.is_anomalous ? "🛑 yes" : "no") <<", reason: " << t.anomaly_reason << "\n"
					  << "    uncertainty radios=" << t.get_uncertainty_radius_m() << "m\n"
					  << "--------------------------------------------------------------------------------" << std::endl;
		}
	}
	return 0;
}