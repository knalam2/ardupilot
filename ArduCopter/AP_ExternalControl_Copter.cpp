/*
  external control library for copter
 */


#include "AP_ExternalControl_Copter.h"
#if AP_EXTERNAL_CONTROL_ENABLED

#include "Copter.h"

/*
  set linear velocity and yaw rate. Pass NaN for yaw_rate_rads to not control yaw
  velocity is in earth frame, NED, m/s
*/
bool AP_ExternalControl_Copter::set_linear_velocity_and_yaw_rate(const Vector3f &linear_velocity, float yaw_rate_rads)
{
    if (!ready_for_external_control()) {
        return false;
    }
    const float yaw_rate_cds = isnan(yaw_rate_rads)? 0: degrees(yaw_rate_rads)*100;

    // Copter velocity is positive if aircraft is moving up which is opposite the incoming NED frame.
    Vector3f velocity_NEU_ms {
        linear_velocity.x,
        linear_velocity.y,
        -linear_velocity.z };
    Vector3f velocity_up_cms = velocity_NEU_ms * 100;
    copter.mode_guided.set_velocity(velocity_up_cms, false, 0, !isnan(yaw_rate_rads), yaw_rate_cds);
    return true;
}

bool AP_ExternalControl_Copter::set_global_position(const Location& loc)
{
    // Check if copter is ready for external control and returns false if it is not.
    if (!ready_for_external_control()) {
        return false;
    }
    return copter.set_target_location(loc);
}

/*
  sets actuator output.
*/
bool AP_ExternalControl_Copter::set_actuator_output(float actuator[AP_MOTORS_MAX_NUM_MOTORS])
{
    if (!ready_for_external_control()) {
        return false;
    }

    copter.mode_guided.set_actuator_mode(actuator);
    return true;
}

bool AP_ExternalControl_Copter::ready_for_external_control()
{
    return copter.flightmode->in_guided_mode() && copter.motors->armed();
}

#endif // AP_EXTERNAL_CONTROL_ENABLED
