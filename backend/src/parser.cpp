#include "aircraft_ping.hpp"
#include <nlohmann/json.hpp>
#include <iostream>

using	json = nlohmann::json;

bool	parse_readsb_line(const std::string& line, aircraft_ping& out_ping) {
	try {
		json j = json::parse(line);
		if (!j.contains("hex") || !j["hex"].is_string())
			return false;
		if (!j.contains("now") || !j["now"].is_number())
			return false;
		if (!j.contains("lat") || !j["lat"].is_number())
			return false;
		if (!j.contains("lon") || !j["lon"].is_number())
			return false;
		if (j.contains("alt_geom") && j["alt_geom"].is_number()) {
			out_ping.alt_m = j["alt_geom"].get<double>() * 0.3048;
		} else if (j.contains("alt_baro") && j["alt_baro"].is_number()) {
			out_ping.alt_m = j["alt_baro"].get<double>() * 0.3048;
		} else {
			return false;
		}
		if (!j.contains("gs") || !j["gs"].is_number())
		{
			out_ping.speed = -1.0;
		} else {
			out_ping.speed = j["gs"].get<double>() * 0.514444;
		}
		if (!j.contains("track") || !j["track"].is_number())
		{
			out_ping.track_deg = -1.0;
		} else {
			out_ping.track_deg = j["track"].get<double>();
		}
		if (j.contains("geom_rate") && j["geom_rate"].is_number()) {
			out_ping.vrate_mps = j["geom_rate"].get<double>() * 0.00508;
		} else if (j.contains("baro_rate") && j["baro_rate"].is_number()) {
			out_ping.vrate_mps = j["baro_rate"].get<double>() * 0.00508;
		} else {
			out_ping.vrate_mps = 0.0;
		}
		if (!j.contains("nic") || !j["nic"].is_number())
		{
			out_ping.nic = -1;
		} else {
			out_ping.nic = j["nic"].get<int>();
		}
		if (!j.contains("nac_p") || !j["nac_p"].is_number())
		{
			out_ping.nac_p = -1;
		} else {
			out_ping.nac_p = j["nac_p"].get<int>();
		}
		if (!j.contains("flight") || !j["flight"].is_string())
		{
			out_ping.callsign = "N/A";
		} else {
			out_ping.callsign = j["flight"].get<std::string>();
		}
		out_ping.hex		= j["hex"].get<std::string>();
		out_ping.timestamp	= j["now"].get<double>();
		out_ping.lat		= j["lat"].get<double>();
		out_ping.lon		= j["lon"].get<double>();
		return true;
	}
	catch(const std::exception& e) {
		std::cerr << e.what() << '\n';
		return false;
	}

}