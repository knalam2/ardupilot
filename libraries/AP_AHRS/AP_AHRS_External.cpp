#include "AP_AHRS_External.h"

#if AP_AHRS_EXTERNAL_ENABLED

#include <AP_ExternalAHRS/AP_ExternalAHRS.h>
#include <AP_AHRS/AP_AHRS.h>

void AP_AHRS_External::update()
{
    AP::externalAHRS().update();
}

void AP_AHRS_External::get_results(AP_AHRS_Backend::Estimates &results)
{
    auto &extahrs = AP::externalAHRS();
    const AP_InertialSensor &_ins = AP::ins();

    results.initialised = extahrs.initialised();
    results.healthy = extahrs.healthy();

    results.primary_imu_index = _ins.get_first_usable_gyro();

    results.attitude_valid = extahrs.get_quaternion(results.quat);

    results.quat.rotation_matrix(results.dcm_matrix);
    results.dcm_matrix = results.dcm_matrix * AP::ahrs().get_rotation_vehicle_body_to_autopilot_body();
    results.dcm_matrix.to_euler(&results.roll_rad, &results.pitch_rad, &results.yaw_rad);

    results.gyro_drift.zero();
    if (!extahrs.get_gyro(results.gyro_estimate)) {
        results.gyro_estimate = _ins.get_gyro();
    }

    Vector3f accel;
    if (!extahrs.get_accel(accel)) {
        accel = _ins.get_accel();
    }

    const Vector3f accel_ef = results.dcm_matrix * AP::ahrs().get_rotation_autopilot_body_to_vehicle_body() * accel;
    results.accel_ef = accel_ef;

    results.velocity_NED_valid = AP::externalAHRS().get_velocity_NED(results.velocity_NED);
    // kinematically-consistent down-rate:
    results.vert_pos_rate_D_valid = AP::externalAHRS().get_speed_down(results.vert_pos_rate_D);

    results.groundspeed_vector = AP::externalAHRS().get_groundspeed_vector();

    results.location_valid = AP::externalAHRS().get_location(results.location);
}

bool AP_AHRS_External::get_relative_position_NED_origin(Vector3f &vec) const
{
    auto &extahrs = AP::externalAHRS();
    Location loc, orgn;
    if (extahrs.get_origin(orgn) &&
        extahrs.get_location(loc)) {
        const Vector2f diff2d = orgn.get_distance_NE(loc);
        vec = Vector3f(diff2d.x, diff2d.y,
                       -(loc.alt - orgn.alt)*0.01);
        return true;
    }
    return false;
}

bool AP_AHRS_External::get_relative_position_NE_origin(Vector2f &posNE) const
{
    auto &extahrs = AP::externalAHRS();

    Location loc, orgn;
    if (!extahrs.get_location(loc) ||
        !extahrs.get_origin(orgn)) {
        return false;
    }
    posNE = orgn.get_distance_NE(loc);
    return true;
}

bool AP_AHRS_External::get_relative_position_D_origin(float &posD) const
{
    auto &extahrs = AP::externalAHRS();

    Location orgn, loc;
    if (!extahrs.get_origin(orgn) ||
        !extahrs.get_location(loc)) {
        return false;
    }
    posD = -(loc.alt - orgn.alt)*0.01;
    return true;
}

bool AP_AHRS_External::pre_arm_check(bool requires_position, char *failure_msg, uint8_t failure_msg_len) const
{
    return AP::externalAHRS().pre_arm_check(failure_msg, failure_msg_len);
}

bool AP_AHRS_External::get_filter_status(nav_filter_status &status) const
{
    AP::externalAHRS().get_filter_status(status);
    return true;
}

void AP_AHRS_External::send_ekf_status_report(GCS_MAVLINK &link) const
{
    AP::externalAHRS().send_status_report(link);
}

bool AP_AHRS_External::get_origin(Location &ret) const
{
    return AP::externalAHRS().get_origin(ret);
}

void AP_AHRS_External::get_control_limits(float &ekfGndSpdLimit, float &ekfNavVelGainScaler) const
{
    // lower gains in VTOL controllers when flying on DCM
    ekfGndSpdLimit = 50.0;
    ekfNavVelGainScaler = 0.5;
}

#endif
