#pragma once
#include <string>

struct	aircraft_ping
{
	std::string	hex;
	std::string	callsign;
	double		timestamp;	//seconds
	double		lat;		//degrees
	double		lon;		//degrees
	double		alt_m;		//meters x 0.3048
	double		speed;		//meters/second x 0.514444
	double		track_deg;	//degrees
	double		vrate_mps;	//meters/second x 0.00508
	int			nic;
	int			nac_p;
};

bool	parse_readsb_line(const std::string& line, aircraft_ping& out_ping);