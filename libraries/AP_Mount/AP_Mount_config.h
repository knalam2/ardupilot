#pragma once

#include <AP_HAL/AP_HAL_Boards.h>
#include <AP_Terrain/AP_Terrain_config.h>
#include <GCS_MAVLink/GCS_config.h>
#include <AP_Scripting/AP_Scripting_config.h>

#ifndef HAL_MOUNT_ENABLED
#define HAL_MOUNT_ENABLED 1
#endif

#ifndef AP_MOUNT_BACKEND_DEFAULT_ENABLED
#define AP_MOUNT_BACKEND_DEFAULT_ENABLED HAL_MOUNT_ENABLED
#endif

#ifndef HAL_MOUNT_ALEXMOS_ENABLED
#define HAL_MOUNT_ALEXMOS_ENABLED AP_MOUNT_BACKEND_DEFAULT_ENABLED
#endif

#ifndef HAL_MOUNT_GREMSY_ENABLED
#define HAL_MOUNT_GREMSY_ENABLED AP_MOUNT_BACKEND_DEFAULT_ENABLED && HAL_GCS_ENABLED && BOARD_FLASH_SIZE > 1024
#endif

#ifndef HAL_MOUNT_SCRIPTING_ENABLED
#define HAL_MOUNT_SCRIPTING_ENABLED AP_MOUNT_BACKEND_DEFAULT_ENABLED && AP_SCRIPTING_ENABLED
#endif

#ifndef HAL_MOUNT_SERVO_ENABLED
#define HAL_MOUNT_SERVO_ENABLED AP_MOUNT_BACKEND_DEFAULT_ENABLED
#endif

#ifndef HAL_MOUNT_SIYI_ENABLED
#define HAL_MOUNT_SIYI_ENABLED AP_MOUNT_BACKEND_DEFAULT_ENABLED
#endif

// solo gimbal is enabled explicitly in hwdefs on some Cubes:
#ifndef HAL_SOLO_GIMBAL_ENABLED
#define HAL_SOLO_GIMBAL_ENABLED 0
#endif

#ifndef HAL_MOUNT_STORM32MAVLINK_ENABLED
#define HAL_MOUNT_STORM32MAVLINK_ENABLED AP_MOUNT_BACKEND_DEFAULT_ENABLED && HAL_GCS_ENABLED
#endif

#ifndef HAL_MOUNT_STORM32SERIAL_ENABLED
#define HAL_MOUNT_STORM32SERIAL_ENABLED AP_MOUNT_BACKEND_DEFAULT_ENABLED
#endif

#ifndef HAL_MOUNT_XACTI_ENABLED
#define HAL_MOUNT_XACTI_ENABLED AP_MOUNT_BACKEND_DEFAULT_ENABLED && HAL_ENABLE_DRONECAN_DRIVERS && BOARD_FLASH_SIZE > 1024
#endif

#ifndef HAL_MOUNT_VIEWPRO_ENABLED
#define HAL_MOUNT_VIEWPRO_ENABLED AP_MOUNT_BACKEND_DEFAULT_ENABLED && BOARD_FLASH_SIZE > 1024
#endif

#ifndef HAL_MOUNT_CADDX_ENABLED
#define HAL_MOUNT_CADDX_ENABLED AP_MOUNT_BACKEND_DEFAULT_ENABLED && BOARD_FLASH_SIZE > 1024
#endif

#ifndef AP_MOUNT_POI_TO_LATLONALT_ENABLED
#define AP_MOUNT_POI_TO_LATLONALT_ENABLED HAL_MOUNT_ENABLED && AP_TERRAIN_AVAILABLE && BOARD_FLASH_SIZE > 1024
#endif

#ifndef HAL_MOUNT_TOPOTEK_ENABLED
#define HAL_MOUNT_TOPOTEK_ENABLED AP_MOUNT_BACKEND_DEFAULT_ENABLED
#endif

// set camera source is supported on gimbals that may have more than one lens
#ifndef HAL_MOUNT_SET_CAMERA_SOURCE_ENABLED
#define HAL_MOUNT_SET_CAMERA_SOURCE_ENABLED HAL_MOUNT_SIYI_ENABLED || HAL_MOUNT_XACTI_ENABLED || HAL_MOUNT_VIEWPRO_ENABLED
#endif

// send thermal range is only support on Siyi cameras
#ifndef AP_MOUNT_SEND_THERMAL_RANGE_ENABLED
#define AP_MOUNT_SEND_THERMAL_RANGE_ENABLED HAL_MOUNT_SIYI_ENABLED
#endif
