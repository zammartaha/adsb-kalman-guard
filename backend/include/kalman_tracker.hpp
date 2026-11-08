#pragma once
#include "enu_projector.hpp"
#include <Eigen/Dense>

using	Vector6d = Eigen::Matrix<double, 6, 1>;
using	Matrix6d = Eigen::Matrix<double, 6, 6>;

const double	init_pos_variance = 100.0;
const double	acc_variance = 4.0;
const double	meas_pos_variance = 900.0;
const double	meas_vel_variance = 25.0;
const double	missing_vel_variance = 1e6;
const double	spoofing_threshold_d2 = 30.0;
const double	TRACK_TIMEOUT_SEC = 60.0;
const double	SWEEP_INTERVAL_SEC = 10.0;

struct	kalman_tracker {
	std::string	hex;
	std::string	callsign;
	double		timestamp;
	int			last_nic;
	int			last_nac_p;

	Vector6d	x;
	Matrix6d	P;
	double		last_mahalanobis; // Spoofing score (d^2)
	bool		is_anomalous;
	std::string	anomaly_reason;

	kalman_tracker() = default;
	kalman_tracker(const aircraft_ping& first_ping, const enu_state& first_enu);

	void	predict(double dt);
	void	update(const aircraft_ping& ping, const enu_state& enu);
	double	get_uncertainty_radius_m() const;
};
