#pragma once
#include "kalman_tracker.hpp"
#include <unordered_map>
#include <chrono>
#include <string>

struct	fleet_manager {
	std::unordered_map<std::string, kalman_tracker>	trackers;
	std::unordered_map<std::string, int>			ping_counts;
	std::chrono::steady_clock::time_point			last_sweep_time;

	fleet_manager();

	void	prune_stale_trackers(double current_ping_time);
	void	maybe_inject_spoof(const std::string& hex, int count, enu_state& enu) const;
	void	prepare_and_print(const aircraft_ping& ping, const enu_state& enu, const kalman_tracker& t, int count) const;
	void	process_ping(const aircraft_ping& ping);
};