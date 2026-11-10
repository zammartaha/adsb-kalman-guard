#include <cmath>
#include "kalman_tracker.hpp"

static Matrix6d	uncertainty_cov(bool has_velocity) {
	Matrix6d P = Matrix6d::Identity() * init_pos_variance;
	if (!has_velocity) {
		P(3,3) = 1000.0 * init_pos_variance;
		P(4,4) = 1000.0 * init_pos_variance;
		P(5,5) = 1000.0 * init_pos_variance;
	}
	return P;
}

static Matrix6d	state_transition(double dt) {
	Matrix6d F = Matrix6d::Identity();
	F(0, 3) = dt;
	F(1, 4) = dt;
	F(2, 5) = dt;
	return F;
}

static Matrix6d	process_noise(double dt) {
	double pos_var = 0.5 * dt * dt * acc_variance;
	double vel_var = dt * acc_variance;

	Matrix6d Q = Matrix6d::Zero();
	Q(0, 0) = pos_var;
	Q(1, 1) = pos_var;
	Q(2, 2) = pos_var;
	Q(3, 3) = vel_var;
	Q(4, 4) = vel_var;
	Q(5, 5) = vel_var;
	return Q;
}

static Matrix6d	meas_noise(bool has_velocity) {
	Matrix6d R = Matrix6d::Zero();
	R(0,0) = meas_pos_variance;
	R(1,1) = meas_pos_variance;
	R(2,2) = meas_pos_variance;

	double vel_var;
	if (has_velocity)
		vel_var = meas_vel_variance;
	else
		vel_var = missing_vel_variance;

	R(3,3) = vel_var;
	R(4,4) = vel_var;
	R(5,5) = vel_var;
	return R;
}

static Vector6d	meas_vector(const enu_state& enu, const Vector6d& predicted_x) {
	Vector6d z;
	if (enu.has_velocity)
		z << enu.x, enu.y, enu.z, enu.vx, enu.vy, enu.vz;
	else
		z << enu.x, enu.y, enu.z, predicted_x(3), predicted_x(4), predicted_x(5);
	return z;
}

kalman_tracker::kalman_tracker(const aircraft_ping& first_ping, const enu_state& first_enu) {
	hex = first_ping.hex;
	callsign = first_ping.callsign;
	timestamp = first_ping.timestamp;
	last_nic = first_ping.nic;
	last_nac_p = first_ping.nac_p;

	last_mahalanobis = 0.0;
	is_anomalous = false;
	anomaly_reason = "none";

	x << first_enu.x, first_enu.y, first_enu.z, first_enu.vx, first_enu.vy, first_enu.vz;
	P = uncertainty_cov(first_enu.has_velocity);
}

double	kalman_tracker::get_uncertainty_radius_m() const {
	return std::sqrt(P(0, 0) + P(1, 1));
}

void	kalman_tracker::predict(double dt) {
	if (dt <= 0.0)
		return;

	Matrix6d F = state_transition(dt);
	Matrix6d Q = process_noise(dt);

	x = F * x;
	P = F * P * F.transpose() + Q;
}

void	kalman_tracker::update(const aircraft_ping& ping, const enu_state& enu) {
	double dt = ping.timestamp - timestamp;
	if (dt < 0.0)
		return;
	predict(dt);
	timestamp = ping.timestamp;
	last_nic = ping.nic;
	last_nac_p = ping.nac_p;
	if (ping.callsign != "N/A")
		callsign = ping.callsign;

	Vector6d z = meas_vector(enu, x);
	Matrix6d R = meas_noise(enu.has_velocity);

	Vector6d y = z - x;				// Diffrence between mesurad and predicted vector
	Matrix6d S = P + R;				// Total expected variance (Prediction uncertainty + measurment noise)
	Matrix6d S_inv = S.inverse();

	last_mahalanobis = (y.transpose() * S_inv * y)(0, 0);

	if (last_mahalanobis > spoofing_threshold_d2) {
		is_anomalous = true;
		anomaly_reason = "KINEMATIC_SPOOF_JUMP";
		return;
	}
	if (ping.nic >= 0 && ping.nic < 5) {
		is_anomalous = true;
		anomaly_reason = "LOW_GNSS_INTEGRITY_NIC";
	} else {
		is_anomalous = false;
		anomaly_reason = "none";
	}

	Matrix6d K = P * S_inv;				// Kalman Gain (0.0 trust prediction, 1.0 trust measurment)
	x = x + K * y;						// Nudge state x toward radio ping by gain K
	P = (Matrix6d::Identity() - K) * P;	// Shrink prediction uncertainty P
}