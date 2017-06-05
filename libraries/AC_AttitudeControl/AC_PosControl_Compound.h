#pragma once

/// @file    AC_AttitudeControl_Compound.h
/// @brief   ArduCopter attitude control library

// control library for forward thrust compound system
// hoping to possibly able to used for some new airframe with lateral thruster

#include "AC_PosControl.h"
#include "AC_AttitudeControl.h"
#include <AP_Motors/AP_MotorsMulticopter.h>
#include <AC_PID/AC_PID.h>

class AC_PosControl_Compound : public AC_PosControl {
public:
  AC_PosControl_Compound(const AP_AHRS_View& ahrs, const AP_InertialNav& inav,
                AP_Motors& motors, AC_AttitudeControl& attitude_control,
                AC_P& p_pos_z, AC_P& p_vel_z, AC_PID& pid_accel_z,
                AC_P& p_pos_xy, AC_PI_2D& pi_vel_xy);
  //AC_AttitudeControl_Compound(AP_AHRS_View &ahrs, const AP_Vehicle::MultiCopter &aparm, AP_MotorsMulticopter& motors, float dt);
	//AC_AttitudeControl_Multi(AP_AHRS_View &ahrs, const AP_Vehicle::MultiCopter &aparm, AP_MotorsMulticopter& motors, float dt);

  //AC_PID& throttle_pid = 0;

  const AP_AHRS_View &        _ahrs;
  //const AP_InertialNav&       _inav;
  AP_Motors&            _motors;
 //AC_AttitudeControl&         _attitude_control;

  // empty destructor to suppress compiler warning
  virtual ~AC_PosControl_Compound() {}

  //enable use of thruster from radio inputs;
  //void set_radio_passthrough_auxiliary_thruster(float forward_radio_passthrough);
  //switch to accel forward to use thruster instead of pitch down.
  virtual void accel_to_lean_angles(float dt_xy, float ekfNavVelGainScaler, bool use_althold_lean_angle);

  // accel forward command to rear thruster throttle
  void run_auxiliary_thruster_controller(float accel_forward);

  float _thrust_out;  // scaled throttle to be pass to motor controller.
};
