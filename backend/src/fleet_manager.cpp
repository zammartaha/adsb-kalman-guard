#include "fleet_manager.hpp"
#include "enu_projector.hpp"
#include <nlohmann/json.hpp>
#include <iostream>
#include <iomanip>

using	json = nlohmann::json;

fleet_manager::fleet_manager() {
	last_sweep_time = std::chrono::steady_clock::now();
}

void	fleet_manager::prune_stale_trackers(double current_ping_time) {
	auto it = trackers.begin();
	while (it != trackers.end()) {
		if (current_ping_time - it->second.timestamp > TRACK_TIMEOUT_SEC) {
			ping_counts.erase(it->first);
			it = trackers.erase(it);
		} else
			++it;
	}
}

void	fleet_manager::maybe_inject_spoof(const std::string& hex, int count, enu_state& enu) const {
	if ((hex == spoof_target || spoof_target == "ANY") && count >= 3 && count <= 50) {
				std::cerr << "\n[⚠️ INJECTING FAKE SPOOF ATTACK ON " << hex << " (+5000m East, -5000m North)]\n";
				enu.x += 5000.0;
				enu.y -= 5000.0;
			}
}

void	fleet_manager::prepare_and_print(const aircraft_ping& ping, const enu_state& enu, const kalman_tracker& t, int count) const {
	double filt_lat = 0.0, filt_lon = 0.0, filt_alt = 0.0;
			enu_to_latlon(t.x(0), t.x(1), t.x(2), filt_lat, filt_lon, filt_alt);

			double meas_lat = 0.0, meas_lon = 0.0, meas_alt = 0.0;
			enu_to_latlon(enu.x, enu.y, enu.z, meas_lat, meas_lon, meas_alt);

			json out = {
				{"hex", t.hex},
				{"callsign", t.callsign},
				{"timestamp", ping.timestamp},
				{"ping_count", count},
				{"raw_lat", meas_lat},
				{"raw_lon", meas_lon},
				{"raw_alt_m", meas_alt},
				{"filtered_lat", filt_lat},
				{"filtered_lon", filt_lon},
				{"filtered_alt_m", filt_alt},
				{"vx", t.x(3)},
				{"vy", t.x(4)},
				{"vz", t.x(5)},
				{"uncertainty_m", t.get_uncertainty_radius_m()},
				{"mahalanobis_d2", t.last_mahalanobis},
				{"is_anomalous", t.is_anomalous},
				{"anomaly_reason", t.anomaly_reason}
			};

			std::cerr << std::fixed << std::setprecision(2)
					  << "[" << count << "] ICAO: " << ping.hex
					  << " | Call: " << ping.callsign << "\n"
					  << "    timestamp= " << ping.timestamp << "\n"
					  << "    Raw      ENU Pos (m)  : East(x)=" << enu.x << ", North(y)=" << enu.y << ", Up(z)=" << enu.z << "\n"
					  << "    Filtered ENU Pos (m)  : East(x)=" << t.x(0) << ", North(y)=" << t.x(1) << ", Up(z)=" << t.x(2) << "\n"
					  << "    Raw      ENU Vel (m/s): vx=" << enu.vx << ", vy=" << enu.vy << ", vz=" << enu.vz << "\n"
					  << "    Filtered ENU Vel (m/s): vx=" << t.x(3) << ", vy=" << t.x(4) << ", vz=" << t.x(5) << "\n"
					  << "    ping have velocety : " << (enu.has_velocity ? "yes" : "no") << "\n"
					  << "    spoofing score=" << t.last_mahalanobis << ", is spoofed: " << (t.is_anomalous ? "🛑 yes" : "no") <<", reason: " << t.anomaly_reason << "\n"
					  << "    uncertainty radios=" << t.get_uncertainty_radius_m() << "m" << std::endl;

			std::cout << out.dump() << std::endl;
			std::cerr << "--------------------------------------------------------------------------------" << std::endl;
}

void	fleet_manager::process_ping(const aircraft_ping& ping) {
	auto now = std::chrono::steady_clock::now();
	double elapsed_time = std::chrono::duration<double>(now - last_sweep_time).count();
	if (elapsed_time >= SWEEP_INTERVAL_SEC) {
		prune_stale_trackers(ping.timestamp);
		last_sweep_time = now;
	}
	enu_state enu = ping_to_enu(ping);
	int& count = ping_counts[ping.hex];
	count++;
	auto it = trackers.find(ping.hex);
	maybe_inject_spoof(ping.hex, ping_counts[ping.hex], enu);
	if (it == trackers.end()) {
		it = trackers.emplace(ping.hex, kalman_tracker(ping, enu)).first; //this line is optemized by AI
		count = 1;
	} else if ((ping.timestamp - it->second.timestamp) > TRACK_TIMEOUT_SEC) {
		it->second = kalman_tracker(ping, enu);
		count = 1;
	} else
		it->second.update(ping, enu);
	prepare_and_print(ping, enu, it->second, count);
}