//  GCS Message ID's
/// NOTE: to ensure we never block on sending MAVLink messages
/// please keep each MSG_ to a single MAVLink message. If need be
/// create new MSG_ IDs for additional messages on the same
/// stream

#pragma once

#include "GCS_config.h"

#include <AP_AHRS/AP_AHRS_config.h>
#include <AP_GPS/AP_GPS_config.h>

enum ap_message : uint8_t {
    MSG_HEARTBEAT,
#if AP_AHRS_ENABLED
    MSG_AHRS,
    MSG_AHRS2,
    MSG_ATTITUDE,
    MSG_ATTITUDE_QUATERNION,
    MSG_LOCATION,
    MSG_VFR_HUD,
#endif
    MSG_SYS_STATUS,
    MSG_POWER_STATUS,
    MSG_MEMINFO,
    MSG_NAV_CONTROLLER_OUTPUT,
    MSG_CURRENT_WAYPOINT,
    MSG_SERVO_OUTPUT_RAW,
    MSG_RC_CHANNELS,
    MSG_RC_CHANNELS_RAW,
    MSG_RAW_IMU,
    MSG_SCALED_IMU,
    MSG_SCALED_IMU2,
    MSG_SCALED_IMU3,
    MSG_SCALED_PRESSURE,
    MSG_SCALED_PRESSURE2,
    MSG_SCALED_PRESSURE3,
    MSG_GPS_RAW,
    MSG_GPS_RTK,
    MSG_GPS2_RAW,
    MSG_GPS2_RTK,
    MSG_SYSTEM_TIME,
    MSG_SERVO_OUT,
    MSG_NEXT_MISSION_REQUEST_WAYPOINTS,
    MSG_NEXT_MISSION_REQUEST_RALLY,
    MSG_NEXT_MISSION_REQUEST_FENCE,
    MSG_NEXT_PARAM,
    MSG_FENCE_STATUS,
    MSG_SIMSTATE,
    MSG_SIM_STATE,
    MSG_HWSTATUS,
    MSG_WIND,
    MSG_RANGEFINDER,
    MSG_DISTANCE_SENSOR,
    MSG_TERRAIN_REQUEST,
    MSG_TERRAIN_REPORT,
    MSG_BATTERY2,
    MSG_CAMERA_FEEDBACK,
    MSG_CAMERA_INFORMATION,
    MSG_CAMERA_SETTINGS,
    MSG_CAMERA_FOV_STATUS,
    MSG_CAMERA_CAPTURE_STATUS,
    MSG_CAMERA_THERMAL_RANGE,
    MSG_GIMBAL_DEVICE_ATTITUDE_STATUS,
    MSG_GIMBAL_MANAGER_INFORMATION,
    MSG_GIMBAL_MANAGER_STATUS,
    MSG_VIDEO_STREAM_INFORMATION,
    MSG_OPTICAL_FLOW,
    MSG_MAG_CAL_PROGRESS,
    MSG_MAG_CAL_REPORT,
    MSG_EKF_STATUS_REPORT,
    MSG_LOCAL_POSITION,
    MSG_PID_TUNING,
    MSG_VIBRATION,
    MSG_RPM,
    MSG_WHEEL_DISTANCE,
    MSG_MISSION_ITEM_REACHED,
    MSG_POSITION_TARGET_GLOBAL_INT,
    MSG_POSITION_TARGET_LOCAL_NED,
    MSG_ADSB_VEHICLE,
    MSG_BATTERY_STATUS,
    MSG_AOA_SSA,
    MSG_LANDING,
    MSG_ESC_TELEMETRY,
    MSG_ORIGIN,
    MSG_HOME,
    MSG_NAMED_FLOAT,
    MSG_EXTENDED_SYS_STATE,
    MSG_AUTOPILOT_VERSION,
    MSG_EFI_STATUS,
    MSG_GENERATOR_STATUS,
    MSG_WINCH_STATUS,
    MSG_WATER_DEPTH,
    MSG_HIGH_LATENCY2,
    MSG_AIS_VESSEL,
    MSG_MCU_STATUS,
    MSG_UAVIONIX_ADSB_OUT_STATUS,
    MSG_ATTITUDE_TARGET,
    MSG_HYGROMETER,
    MSG_AUTOPILOT_STATE_FOR_GIMBAL_DEVICE,
    MSG_RELAY_STATUS,
#if AP_MAVLINK_MSG_HIGHRES_IMU_ENABLED
    MSG_HIGHRES_IMU,
#endif
    MSG_AIRSPEED,
#if AP_GPS_GNSS_SENDING_ENABLED
    MSG_GNSS,
#endif
    MSG_LAST // MSG_LAST must be the last entry in this enum
};
