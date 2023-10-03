#include <AP_HAL/AP_HAL_Boards.h>

#if AP_UROS_ENABLED

#include "AP_UROS_Client.h"
#include "AP_UROS_Type_Conversions.h"

#include <algorithm>
#include <string.h>

// micro-ROS
#include <micro_ros_utilities/type_utilities.h>
#include <rmw_microros/rmw_microros.h>

#if CONFIG_HAL_BOARD == HAL_BOARD_ESP32
#include <driver/uart.h>
#endif

#include <AP_AHRS/AP_AHRS.h>
#include <AP_Arming/AP_Arming.h>
#include <AP_BattMonitor/AP_BattMonitor.h>
#include <AP_GPS/AP_GPS.h>
#include <AP_HAL/AP_HAL.h>
#include <AP_Math/AP_Math.h>
#include <AP_RTC/AP_RTC.h>
#include <AP_Vehicle/AP_Vehicle.h>
#include <GCS_MAVLink/GCS.h>

#include <AP_ROS/AP_ROS_Client.h>

#include <AP_ExternalControl/AP_ExternalControl_config.h>
#if AP_EXTERNAL_CONTROL_ENABLED
#include "AP_UROS_ExternalControl.h"
#endif
// #include "AP_UROS_Frames.h"
#include "AP_UROS_External_Odom.h"

#define UROS_DEBUG 1
#define UROS_INFO 1
#define UROS_ERROR 1

#if UROS_DEBUG
#define uros_debug(fmt, args...) do {\
  hal.console->printf(fmt "\n", ##args);\
  GCS_SEND_TEXT(MAV_SEVERITY_DEBUG, fmt, ##args);\
} while(0)
#else
#define uros_debug(fmt, args...) do {} while(0)
#endif

#if UROS_INFO
#define uros_info(fmt, args...) do {\
  hal.console->printf(fmt "\n", ##args);\
  GCS_SEND_TEXT(MAV_SEVERITY_INFO, fmt, ##args);\
} while(0)
#else
#define uros_debug(fmt, args...) do {} while(0)
#endif

#if UROS_ERROR
#define uros_error(fmt, args...) do {\
  hal.console->printf(fmt "\n", ##args);\
  GCS_SEND_TEXT(MAV_SEVERITY_ERROR, fmt, ##args);\
} while(0)
#else
#define uros_error(fmt, args...) do {} while(0)
#endif

#define UROS_CHECK(fn, msg) {\
    rcl_ret_t temp_rc = fn;\
    if ((temp_rc != RCL_RET_OK)) {\
        uros_error("UROS: " msg "... FAILED, line %d: code: %d",\
            __LINE__, (int)temp_rc);\
        /* will be empty when RCUTILS_AVOID_DYNAMIC_ALLOCATION is defined */\
        const rcutils_error_state_t *err = rcutils_get_error_state();\
        uros_error("UROS: error: %s, file: %s, line: %u, ",\
            err->message, err->file, (uint32_t)err->line_number);\
        return false;\
    } else {\
        uros_debug("UROS: " msg"... OK");\
    }\
}

#define UROS_SOFTCHECK(fn) {\
    rcl_ret_t temp_rc = fn;\
    if ((temp_rc != RCL_RET_OK)) {\
        uros_info("UROS: failed status, line %d: code: %d",\
            __LINE__, (int)temp_rc);\
    }\
}

// publishers
constexpr uint16_t DELAY_BATTERY_STATE_TOPIC_MS = 1000;
constexpr uint16_t DELAY_CLOCK_TOPIC_MS = 10;
constexpr uint16_t DELAY_GEO_POSE_TOPIC_MS = 33;
constexpr uint16_t DELAY_LOCAL_POSE_TOPIC_MS = 33;
constexpr uint16_t DELAY_LOCAL_TWIST_TOPIC_MS = 33;
constexpr uint16_t DELAY_STATIC_TRANSFORM_TOPIC_MS = 1000;
constexpr uint16_t DELAY_TIME_TOPIC_MS = 10;

// constructor
AP_UROS_Client::AP_UROS_Client() {
    if (_singleton) {
        uros_error("PANIC: Too many AP_UROS_Client instances");
    }
    _singleton = this;
}

// singleton
AP_UROS_Client *AP_UROS_Client::_singleton = nullptr;

AP_UROS_Client *AP_UROS_Client::get_singleton() {
    return AP_UROS_Client::_singleton;
}

// update published topics

void AP_UROS_Client::update_topic(sensor_msgs__msg__BatteryState& msg)
{
    const uint8_t instance = 0;
    AP_ROS_Client::update_battery_state(msg, instance);
}

void AP_UROS_Client::update_topic(rosgraph_msgs__msg__Clock& msg)
{
    AP_ROS_Client::update_clock(msg);
}

void AP_UROS_Client::update_topic(geographic_msgs__msg__GeoPoseStamped& msg)
{
    AP_ROS_Client::update_geopose_stamped(msg);
}

void AP_UROS_Client::update_topic(geometry_msgs__msg__PoseStamped& msg)
{
    AP_ROS_Client::update_pose_stamped(msg);
}

void AP_UROS_Client::update_topic(geometry_msgs__msg__TwistStamped& msg)
{
    AP_ROS_Client::update_twist_stamped(msg);
}

void AP_UROS_Client::update_topic(tf2_msgs__msg__TFMessage& msg)
{
    AP_ROS_Client::update_static_transforms(msg);
}

bool AP_UROS_Client::update_topic(sensor_msgs__msg__NavSatFix& msg)
{
    const uint8_t instance = 0;
    return AP_ROS_Client::update_nav_sat_fix(msg, instance, last_nav_sat_fix_time_ms);
}

void AP_UROS_Client::update_topic(builtin_interfaces__msg__Time& msg)
{
    AP_ROS_Client::update_time(msg);
}

// call publishers
void AP_UROS_Client::timer_callback_trampoline(rcl_timer_t * timer, int64_t last_call_time)
{
    AP_UROS_Client *uros = AP_UROS_Client::get_singleton();
    uros->timer_callback(timer, last_call_time);
}

void AP_UROS_Client::timer_callback(rcl_timer_t * timer, int64_t last_call_time)
{
    (void) last_call_time;

    WITH_SEMAPHORE(csem);

    // get the timer clock
    rcl_clock_t * clock;
    UROS_SOFTCHECK(rcl_timer_clock(timer, &clock));

    //! @todo(srmainwaring) - use the rcl_clock?
    const auto cur_time_ms = AP_HAL::millis64();

    if (timer != NULL) {

        if (battery_state_pub_init && 
            cur_time_ms - last_battery_state_time_ms > DELAY_BATTERY_STATE_TOPIC_MS) {
            update_topic(battery_state_msg);
            last_battery_state_time_ms = cur_time_ms;
            UROS_SOFTCHECK(rcl_publish(&battery_state_publisher, &battery_state_msg, NULL));
        }

        if (clock_pub_init &&
            cur_time_ms - last_clock_time_ms > DELAY_CLOCK_TOPIC_MS) {
            update_topic(clock_msg);
            last_clock_time_ms = cur_time_ms;
            UROS_SOFTCHECK(rcl_publish(&clock_publisher, &clock_msg, NULL));
        }

        if (local_pose_pub_init &&
            cur_time_ms - last_local_pose_time_ms > DELAY_LOCAL_POSE_TOPIC_MS) {
            update_topic(local_pose_msg);
            last_local_pose_time_ms = cur_time_ms;
            UROS_SOFTCHECK(rcl_publish(&local_pose_publisher, &local_pose_msg, NULL));
        }

        if (local_twist_pub_init &&
            cur_time_ms - last_local_twist_time_ms > DELAY_LOCAL_TWIST_TOPIC_MS) {
            update_topic(local_twist_msg);
            last_local_twist_time_ms = cur_time_ms;
            UROS_SOFTCHECK(rcl_publish(&local_twist_publisher, &local_twist_msg, NULL));
        }

        if (nav_sat_fix_pub_init &&
            update_topic(nav_sat_fix_msg)) {
            UROS_SOFTCHECK(rcl_publish(&nav_sat_fix_publisher, &nav_sat_fix_msg, NULL));
        }

        if (tx_static_transforms_pub_init &&
            cur_time_ms - last_tx_static_transforms_time_ms > DELAY_STATIC_TRANSFORM_TOPIC_MS) {
            update_topic(tx_static_transforms_msg);
            last_tx_static_transforms_time_ms = cur_time_ms;
            UROS_SOFTCHECK(rcl_publish(&tx_static_transforms_publisher, &tx_static_transforms_msg, NULL));
        }

        if (geo_pose_pub_init &&
            cur_time_ms - last_geo_pose_time_ms > DELAY_GEO_POSE_TOPIC_MS) {
            update_topic(geo_pose_msg);
            last_geo_pose_time_ms = cur_time_ms;
            UROS_SOFTCHECK(rcl_publish(&geo_pose_publisher, &geo_pose_msg, NULL));
        }

        if (time_pub_init &&
            cur_time_ms - last_time_time_ms > DELAY_TIME_TOPIC_MS) {
            update_topic(time_msg);
            last_time_time_ms = cur_time_ms;
            UROS_SOFTCHECK(rcl_publish(&time_publisher, &time_msg, NULL));
            // hal.console->printf("UROS: sent time: %d, %d\n", time_msg.sec, time_msg.nanosec);
        }
    }
}

// subscriber callbacks
void AP_UROS_Client::on_joy_msg_trampoline(const void * msgin, void* context)
{
    AP_UROS_Client *uros = (AP_UROS_Client*)context;
    const sensor_msgs__msg__Joy *msg = (const sensor_msgs__msg__Joy *)msgin;
    uros->on_joy_msg(msg);
}

void AP_UROS_Client::on_joy_msg(const sensor_msgs__msg__Joy *msg)
{
    //! @todo(srmainwaring) implement joystick RC control to AP
    if (msg->axes.size >= 4) {
        uros_info("UROS: sensor_msgs/Joy: %f, %f, %f, %f",
            msg->axes.data[0], msg->axes.data[1], msg->axes.data[2], msg->axes.data[3]);
    } else {
        uros_error("UROS: sensor_msgs/Joy must have axes size >= 4");
    }
}

void AP_UROS_Client::on_velocity_control_msg_trampoline(const void * msgin, void* context)
{
    AP_UROS_Client *uros = (AP_UROS_Client*)context;
    const geometry_msgs__msg__TwistStamped *msg = (const geometry_msgs__msg__TwistStamped *)msgin;
    uros->on_velocity_control_msg(msg);
}

void AP_UROS_Client::on_velocity_control_msg(const geometry_msgs__msg__TwistStamped *msg)
{
#if AP_EXTERNAL_CONTROL_ENABLED
    if (!AP_UROS_External_Control::handle_velocity_control(*msg)) {
        // TODO #23430 handle velocity control failure through rosout, throttled.
    }
#endif // AP_EXTERNAL_CONTROL_ENABLED
}

void AP_UROS_Client::on_tf_msg_trampoline(const void * msgin, void* context)
{
    AP_UROS_Client *uros = (AP_UROS_Client*)context;
    const tf2_msgs__msg__TFMessage *msg = (const tf2_msgs__msg__TFMessage *)msgin;
    uros->on_tf_msg(msg);
}

void AP_UROS_Client::on_tf_msg(const tf2_msgs__msg__TFMessage * msg)
{
    if (msg->transforms.size > 0) {
#if AP_UROS_VISUALODOM_ENABLED
        AP_UROS_External_Odom::handle_external_odom(*msg);
#endif // AP_UROS_VISUALODOM_ENABLED
    } else {
        uros_error("UROS: tf2_msgs/TFMessage with no content");
    }
}

// service callbacks
void AP_UROS_Client::arm_motors_callback_trampoline(
    const void *req, void *res, void *context)
{
    AP_UROS_Client *uros = (AP_UROS_Client*)context;
        ardupilot_msgs__srv__ArmMotors_Request *req_in =
        (ardupilot_msgs__srv__ArmMotors_Request *) req;
    ardupilot_msgs__srv__ArmMotors_Response *res_in =
        (ardupilot_msgs__srv__ArmMotors_Response *) res;
    uros->arm_motors_callback(req_in, res_in);
}

void AP_UROS_Client::arm_motors_callback(
    const ardupilot_msgs__srv__ArmMotors_Request *req,
    ardupilot_msgs__srv__ArmMotors_Response *res)
{
    if (req->arm) {
        uros_info("UROS: Request for arming received");
        res->result = AP::arming().arm(AP_Arming::Method::DDS);
    } else {
        uros_info("UROS: Request for disarming received");
        res->result = AP::arming().disarm(AP_Arming::Method::DDS);
    }

    if (res->result) {
        uros_info("UROS: Request for Arming/Disarming : SUCCESS");
    } else {
        uros_info("UROS: Request for Arming/Disarming : FAIL");
    }
}

void AP_UROS_Client::mode_switch_callback_trampoline(
    const void *req, void *res, void *context)
{
    AP_UROS_Client *uros = (AP_UROS_Client*)context;
        ardupilot_msgs__srv__ModeSwitch_Request *req_in =
        (ardupilot_msgs__srv__ModeSwitch_Request *) req;
    ardupilot_msgs__srv__ModeSwitch_Response *res_in =
        (ardupilot_msgs__srv__ModeSwitch_Response *) res;
    uros->mode_switch_callback(req_in, res_in);
}

void AP_UROS_Client::mode_switch_callback(
    const ardupilot_msgs__srv__ModeSwitch_Request *req,
    ardupilot_msgs__srv__ModeSwitch_Response *res)
{
    res->status = AP::vehicle()->set_mode(req->mode, ModeReason::DDS_COMMAND);
    res->curr_mode = AP::vehicle()->get_mode();

    if (res->status) {
        uros_info("UROS: Request for Mode Switch : SUCCESS");
    } else {
        uros_info("UROS: Request for Mode Switch : FAIL");
    }
}

#if AP_UROS_PARAM_SRV_ENABLED
// parameter server callback
bool AP_UROS_Client::on_parameter_changed_trampoline(
    const Parameter * old_param, const Parameter * new_param, void * context)
{
    AP_UROS_Client *uros = (AP_UROS_Client*)context;
    return uros->on_parameter_changed(old_param, new_param);
}

bool AP_UROS_Client::on_parameter_changed(
    const Parameter * old_param, const Parameter * new_param)
{
    //! @note copied from rcl_examples/src/example_parameter_server.c
    if (old_param == NULL && new_param == NULL) {
        uros_error("UROS: error updating parameters");
        return false;
    }

    if (old_param == NULL) {
        uros_info("UROS: creating new parameter %s",
            new_param->name.data);
    } else if (new_param == NULL) {
        uros_info("UROS: deleting parameter %s",
            old_param->name.data);
    } else {
        switch (old_param->value.type) {
            case RCLC_PARAMETER_BOOL:
                uros_info(
                    "UROS: parameter %s modified: "
                    "old value: %d, new value: %d (bool)",
                    old_param->name.data,
                    old_param->value.bool_value,
                    new_param->value.bool_value);
                break;
            case RCLC_PARAMETER_INT:
                uros_info(
                    "UROS: parameter %s modified: "
                    "old value: %d, new value: %d (int)",
                    old_param->name.data,
                    (int32_t)old_param->value.integer_value,
                    (int32_t)new_param->value.integer_value);
                break;
            case RCLC_PARAMETER_DOUBLE:
                uros_info(
                    "UROS: parameter %s modified: "
                    "old value: %f, new value: %f (double)",
                    old_param->name.data,
                    old_param->value.double_value,
                    new_param->value.double_value);
                break;
            default:
                break;
        }
    }

    return true;
}
#endif

// parameter group
const AP_Param::GroupInfo AP_UROS_Client::var_info[] {

    // @Param: _ENABLE
    // @DisplayName: UROS enable
    // @Description: Enable UROS subsystem
    // @Values: 0:Disabled,1:Enabled
    // @RebootRequired: True
    // @User: Advanced
    AP_GROUPINFO_FLAGS("_ENABLE", 1, AP_UROS_Client, enabled, 0,
        AP_PARAM_FLAG_ENABLE),

#if AP_UROS_UDP_ENABLED
    // @Param: _UDP_PORT
    // @DisplayName: UROS UDP port
    // @Description: UDP port number for UROS
    // @Range: 1 65535
    // @RebootRequired: True
    // @User: Standard
    AP_GROUPINFO("_PORT", 2, AP_UROS_Client, udp.port, 2019),
#endif

    AP_GROUPEND
};

/*
  start the UROS thread
 */
bool AP_UROS_Client::start(void)
{
    AP_Param::setup_object_defaults(this, var_info);
    AP_Param::load_object_from_eeprom(this, var_info);

    if (enabled == 0) {
        return true;
    }

    uros_debug("UROS: creating uros_thread...");

#if CONFIG_HAL_BOARD == HAL_BOARD_ESP32
    BaseType_t rc = xTaskCreatePinnedToCore(
        &AP_UROS_Client::main_loop_trampoline,
        "APM_UROS",
        16384,
        this,
        10, //UROS_PRIO,
        &uros_task_handle, 0);

    if (rc != pdPASS) {
        uros_error("UROS: uros_thread failed to start");
        return false;
    } else {
        uros_info("UROS: uros_thread started");
        return true;
    }
#else
    if (!hal.scheduler->thread_create(
            FUNCTOR_BIND_MEMBER(&AP_UROS_Client::main_loop, void),
            "APM_UROS",
            16384,
            AP_HAL::Scheduler::PRIORITY_IO,
            1)) {
        uros_error("UROS: thread create failed");
        return false;
    }
    return true;
#endif
}

/*
  trampoline for main loop for UROS thread
 */
void AP_UROS_Client::main_loop_trampoline(void *arg) {
    AP_UROS_Client* uros = (AP_UROS_Client*)arg;
    uros->main_loop();

    // if main_loop returns something has gone wrong
    uros_error("UROS: main_thread failed");
    while (true) {
        hal.scheduler->delay(1000);
    }
}

/*
  main loop for UROS thread
 */
void AP_UROS_Client::main_loop(void)
{
    uros_debug("UROS: started main loop");

    if (!init() || !create()) {
        uros_error("UROS: init or create failed");
        return;
    }
    uros_info("UROS: init and create succeed");

    // one-time actions

    // periodic actions
    rclc_executor_spin(&executor);

#if AP_UROS_PARAM_SRV_ENABLED
    UROS_SOFTCHECK(rclc_parameter_server_fini(&param_server, &node));
#endif

    UROS_SOFTCHECK(rcl_service_fini(&arm_motors_service, &node));

    UROS_SOFTCHECK(rcl_subscription_fini(&rx_joy_subscriber, &node));
    UROS_SOFTCHECK(rcl_subscription_fini(&rx_dynamic_transforms_subscriber, &node));

    UROS_SOFTCHECK(rcl_publisher_fini(&battery_state_publisher, &node));
    UROS_SOFTCHECK(rcl_publisher_fini(&clock_publisher, &node));
    UROS_SOFTCHECK(rcl_publisher_fini(&geo_pose_publisher, &node));
    UROS_SOFTCHECK(rcl_publisher_fini(&local_pose_publisher, &node));
    UROS_SOFTCHECK(rcl_publisher_fini(&local_twist_publisher, &node));
    UROS_SOFTCHECK(rcl_publisher_fini(&nav_sat_fix_publisher, &node));
    UROS_SOFTCHECK(rcl_publisher_fini(&tx_static_transforms_publisher, &node));
    UROS_SOFTCHECK(rcl_publisher_fini(&time_publisher, &node));

    UROS_SOFTCHECK(rcl_node_fini(&node));
}

bool AP_UROS_Client::init()
{
    WITH_SEMAPHORE(csem);

    // initialize transport
    bool initTransportStatus = false;

// #if defined(RMW_UXRCE_TRANSPORT_CUSTOM)
    // serial init will fail if the SERIALn_PROTOCOL is not setup
    if (!initTransportStatus) {
        initTransportStatus = urosSerialInit();
    }
// #else
// #error micro-ROS transports misconfigured
// #endif

#if AP_UROS_UDP_ENABLED
    // fallback to UDP if available
    if (!initTransportStatus) {
        initTransportStatus = urosUdpInit();
    }
#endif

    if (initTransportStatus) {
        uros_info("UROS: transport initializated");
    }
    else {
        uros_error("UROS: transport initialization failed");
        return false;
    }

    // create allocator
    allocator = rcl_get_default_allocator();

    // either: custom initialisation
    // uros_debug("UROS: create init options");
    // rcl_init_options_t init_options = rcl_get_zero_initialized_init_options();
    // UROS_CHECK(rcl_init_options_init(&init_options, allocator), "create init options");

    //! @todo add conditional check if using UDP
    // uros_debug("UROS: set rmw init options");
    // rmw_init_options_t* rmw_options = rcl_init_options_get_rmw_init_options(&init_options);
    // UROS_CHECK(rmw_uros_options_set_udp_address("192.168.1.31", "2019", rmw_options),
    //     "set rmw init options");

    // create init_options
    // uros_debug("UROS: initialise support");
    // UROS_CHECK(rclc_support_init_with_options(&support, 0, NULL, &init_options, &allocator),
    //     "initialise support");

    // or: default initialisation
    // create init_options
    // uros_debug("UROS: initialise support");
    UROS_CHECK(rclc_support_init(&support, 0, NULL, &allocator),
        "initialise support");

    // create node
    // uros_debug("UROS: initialise node");
    UROS_CHECK(rclc_node_init_default(&node, "ardupilot_uros", "", &support),
        "initialise node");

    uros_info("UROS: init complete");

    return true;
}

bool AP_UROS_Client::create()
{
    uros_debug("UROS: create entities");

    WITH_SEMAPHORE(csem);

    // initialise strings and sequences
    {
        battery_state_conf.max_string_capacity = 32;
        battery_state_conf.max_ros2_type_sequence_capacity = AP_BATT_MONITOR_CELLS_MAX;
        battery_state_conf.max_basic_type_sequence_capacity = AP_BATT_MONITOR_CELLS_MAX;

        battery_state_mem_init = micro_ros_utilities_create_message_memory(
            ROSIDL_GET_MSG_TYPE_SUPPORT(sensor_msgs, msg, BatteryState),
            &battery_state_msg,
            battery_state_conf
        );
        if (!battery_state_mem_init) {
            uros_error("UROS: configure battery state msg... FAILED");
            return false;
        }
        uros_debug("UROS: configure battery state msg... OK");
    }
    {
        geo_pose_conf.max_string_capacity = 32;
        geo_pose_conf.max_ros2_type_sequence_capacity = 2;
        geo_pose_conf.max_basic_type_sequence_capacity = 2;

        geo_pose_mem_init = micro_ros_utilities_create_message_memory(
            ROSIDL_GET_MSG_TYPE_SUPPORT(geographic_msgs, msg, GeoPoseStamped),
            &geo_pose_msg,
            geo_pose_conf
        );
        if (!geo_pose_mem_init) {
            uros_error("UROS: configure geo pose msg... FAILED");
            return false;
        }
        uros_debug("UROS: configure geo pose msg... OK");
    }
    {
        local_pose_conf.max_string_capacity = 32;
        local_pose_conf.max_ros2_type_sequence_capacity = 2;
        local_pose_conf.max_basic_type_sequence_capacity = 2;

        local_pose_mem_init = micro_ros_utilities_create_message_memory(
            ROSIDL_GET_MSG_TYPE_SUPPORT(geometry_msgs, msg, PoseStamped),
            &local_pose_msg,
            local_pose_conf
        );
        if (!local_pose_mem_init) {
            uros_error("UROS: configure local pose msg... FAILED");
            return false;
        }
        uros_debug("UROS: configure local pose msg... OK");
    }
    {
        local_twist_conf.max_string_capacity = 32;
        local_twist_conf.max_ros2_type_sequence_capacity = 2;
        local_twist_conf.max_basic_type_sequence_capacity = 2;

        local_twist_mem_init = micro_ros_utilities_create_message_memory(
            ROSIDL_GET_MSG_TYPE_SUPPORT(geometry_msgs, msg, TwistStamped),
            &local_twist_msg,
            local_twist_conf
        );
        if (!local_twist_mem_init) {
            uros_error("UROS: configure local twist msg... FAILED");
            return false;
        }
        uros_debug("UROS: configure local twist msg... OK");
    }
    {
        nav_sat_fix_conf.max_string_capacity = 32;
        nav_sat_fix_conf.max_ros2_type_sequence_capacity = 2;
        nav_sat_fix_conf.max_basic_type_sequence_capacity = 2;

        nav_sat_fix_mem_init = micro_ros_utilities_create_message_memory(
            ROSIDL_GET_MSG_TYPE_SUPPORT(sensor_msgs, msg, NavSatFix),
            &nav_sat_fix_msg,
            nav_sat_fix_conf
        );
        if (!nav_sat_fix_mem_init) {
            uros_error("UROS: configure nav sat fix msg... FAILED");
            return false;
        }
        uros_debug("UROS: configure nav sat fix msg... OK");
    }
    {
        tx_static_transforms_conf.max_string_capacity = 32;
        tx_static_transforms_conf.max_ros2_type_sequence_capacity = GPS_MAX_RECEIVERS;
        tx_static_transforms_conf.max_basic_type_sequence_capacity = GPS_MAX_RECEIVERS;

        tx_static_transforms_mem_init = micro_ros_utilities_create_message_memory(
            ROSIDL_GET_MSG_TYPE_SUPPORT(tf2_msgs, msg, TFMessage),
            &tx_static_transforms_msg,
            tx_static_transforms_conf
        );
        if (!nav_sat_fix_mem_init) {
            uros_error("UROS: configure static transform msg... FAILED");
            return false;
        }
        uros_debug("UROS: configure static transform msg... OK");
    }
    {
        rx_joy_conf.max_string_capacity = 32;
        rx_joy_conf.max_ros2_type_sequence_capacity = 32;
        rx_joy_conf.max_basic_type_sequence_capacity = 32;

        rx_joy_mem_init = micro_ros_utilities_create_message_memory(
            ROSIDL_GET_MSG_TYPE_SUPPORT(sensor_msgs, msg, Joy),
            &rx_joy_msg,
            rx_joy_conf
        );
        if (!rx_joy_mem_init) {
            uros_error("UROS: configure joy msg... FAILED");
            return false;
        }
        uros_debug("UROS: configure joy msg... OK");
    }
    {
        rx_velocity_control_conf.max_string_capacity = 32;
        rx_velocity_control_conf.max_ros2_type_sequence_capacity = 32;
        rx_velocity_control_conf.max_basic_type_sequence_capacity = 2;

        rx_velocity_control_mem_init = micro_ros_utilities_create_message_memory(
            ROSIDL_GET_MSG_TYPE_SUPPORT(geometry_msgs, msg, TwistStamped),
            &rx_velocity_control_msg,
            rx_velocity_control_conf
        );
        if (!rx_velocity_control_mem_init) {
            uros_error("UROS: configure velocity control msg... FAILED");
            return false;
        }
        uros_debug("UROS: configure velocity control msg... OK");
    }
    {
        rx_dynamic_transforms_conf.max_string_capacity = 32;
        rx_dynamic_transforms_conf.max_ros2_type_sequence_capacity = 16;
        rx_dynamic_transforms_conf.max_basic_type_sequence_capacity = 16;

        rx_dynamic_transforms_mem_init = micro_ros_utilities_create_message_memory(
            ROSIDL_GET_MSG_TYPE_SUPPORT(tf2_msgs, msg, TFMessage),
            &rx_dynamic_transforms_msg,
            rx_dynamic_transforms_conf
        );
        if (!rx_dynamic_transforms_mem_init) {
            uros_error("UROS: configure tf msg... FAILED");
            return false;
        }
        uros_debug("UROS: configure tf msg... OK");
    }

    // create publishers
    uint8_t number_of_publishers = 0;

    if (battery_state_mem_init) {
        rcl_ret_t rc = rclc_publisher_init_default(
            &battery_state_publisher,
            &node,
            ROSIDL_GET_MSG_TYPE_SUPPORT(sensor_msgs, msg, BatteryState),
            "ap/battery/battery0");
        if (!(battery_state_pub_init = (rc == RCL_RET_OK))) {
            uros_error("UROS: create battery state publisher... FAILED (%d)", (int16_t)rc);
            return false;
        }
        number_of_publishers++;
        uros_debug("UROS: create battery state publisher... OK");
    }

    {
        rcl_ret_t rc = rclc_publisher_init_default(
            &clock_publisher,
            &node,
            ROSIDL_GET_MSG_TYPE_SUPPORT(rosgraph_msgs, msg, Clock),
            "ap/clock");
        if (!(clock_pub_init = (rc == RCL_RET_OK))) {
            uros_error("UROS: create clock publisher... FAILED (%d)", (int16_t)rc);
            return false;
        }
        number_of_publishers++;
        uros_debug("UROS: create clock publisher... OK");
    }

    if (geo_pose_mem_init) {
      rcl_ret_t rc = rclc_publisher_init_default(
          &geo_pose_publisher,
          &node,
          ROSIDL_GET_MSG_TYPE_SUPPORT(geographic_msgs, msg, GeoPoseStamped),
          "ap/geopose/filtered");
        if (!(geo_pose_pub_init = (rc == RCL_RET_OK))) {
            uros_error("UROS: create geo pose publisher... FAILED (%d)",(int16_t)rc);
            return false;
        }
        number_of_publishers++;
        uros_debug("UROS: create geo pose publisher... OK");
    }

    if (local_pose_mem_init) {
        rcl_ret_t rc = rclc_publisher_init_default(
            &local_pose_publisher,
            &node,
            ROSIDL_GET_MSG_TYPE_SUPPORT(geometry_msgs, msg, PoseStamped),
            "ap/pose/filtered");
        if (!(local_pose_pub_init = (rc == RCL_RET_OK))) {
            uros_error("UROS: create local pose publisher... FAILED (%d)", (int16_t)rc);
            return false;
        }
        number_of_publishers++;
        uros_debug("UROS: create local pose publisher... OK");
    }

    if (local_twist_mem_init) {
        rcl_ret_t rc = rclc_publisher_init_default(
            &local_twist_publisher,
            &node,
            ROSIDL_GET_MSG_TYPE_SUPPORT(geometry_msgs, msg, TwistStamped),
            "ap/twist/filtered");
        if (!(local_twist_pub_init = (rc == RCL_RET_OK))) {
            uros_error("UROS: create local twist publisher... FAILED (%d)", (int16_t)rc);
            return false;
        }
        number_of_publishers++;
        uros_debug("UROS: create local twist publisher... OK");
    }

    if (nav_sat_fix_mem_init) {
        rcl_ret_t rc = rclc_publisher_init_default(
            &nav_sat_fix_publisher,
            &node,
            ROSIDL_GET_MSG_TYPE_SUPPORT(sensor_msgs, msg, NavSatFix),
            "ap/navsat/navsat0");
        if (!(nav_sat_fix_pub_init = (rc == RCL_RET_OK))) {
            uros_error("UROS: create nav sat fix publisher... FAILED (%d)", (int16_t)rc);
            return false;
        }
        number_of_publishers++;
        uros_debug("UROS: create nav sat fix publisher... OK");
    }

    if (tx_static_transforms_mem_init) {
        rcl_ret_t rc = rclc_publisher_init_default(
            &tx_static_transforms_publisher,
            &node,
            ROSIDL_GET_MSG_TYPE_SUPPORT(tf2_msgs, msg, TFMessage),
            "ap/tf_static");
        if (!(tx_static_transforms_pub_init = (rc == RCL_RET_OK))) {
            uros_error("UROS: create static transform publisher... FAILED (%d)", (int16_t)rc);
            return false;
        }
        number_of_publishers++;
        uros_debug("UROS: create static transform publisher... OK");
    }

    {
        rcl_ret_t rc = rclc_publisher_init_default(
            &time_publisher,
            &node,
            ROSIDL_GET_MSG_TYPE_SUPPORT(builtin_interfaces, msg, Time),
            "ap/time");
        if (!(time_pub_init = (rc == RCL_RET_OK))) {
            uros_error("UROS: create time publisher... FAILED (%d)", (int16_t)rc);
            return false;
        }
        number_of_publishers++;
        uros_debug("UROS: create time publisher... OK");
    }

    // create subscribers
    uint8_t number_of_subscribers = 0;

    if (rx_joy_mem_init) {
        rcl_ret_t rc = rclc_subscription_init_default(
            &rx_joy_subscriber,
            &node,
            ROSIDL_GET_MSG_TYPE_SUPPORT(sensor_msgs, msg, Joy),
            "ap/joy");
        if (!(rx_joy_sub_init = (rc == RCL_RET_OK))) {
            uros_error("UROS: create joy subscriber... FAILED (%d)", (int16_t)rc);
            return false;
        }
        number_of_subscribers++;
        uros_debug("UROS: create joy subscriber... OK");
    }

    if (rx_velocity_control_mem_init) {
        rcl_ret_t rc = rclc_subscription_init_default(
            &rx_velocity_control_subscriber,
            &node,
            ROSIDL_GET_MSG_TYPE_SUPPORT(geometry_msgs, msg, TwistStamped),
            "ap/cmd_vel");
        if (!(rx_velocity_control_sub_init = (rc == RCL_RET_OK))) {
            uros_error("UROS: create velocity control subscriber... FAILED (%d)", (int16_t)rc);
            return false;
        }
        number_of_subscribers++;
        uros_debug("UROS: create velocity control subscriber... OK");
    }

    if (rx_dynamic_transforms_mem_init) {
        rcl_ret_t rc = rclc_subscription_init_default(
            &rx_dynamic_transforms_subscriber,
            &node,
            ROSIDL_GET_MSG_TYPE_SUPPORT(tf2_msgs, msg, TFMessage),
            "ap/tf");
        if (!(rx_dynamic_transforms_sub_init = (rc == RCL_RET_OK))) {
            uros_error("UROS: create tf subscriber... FAILED (%d)", (int16_t)rc);
            return false;
        }
        number_of_subscribers++;
        uros_debug("UROS: create tf subscriber... OK");
    }

    // create services
    uint8_t number_of_services = 0;

    {
        rcl_ret_t rc = rclc_service_init_default(
            &arm_motors_service,
            &node,
            ROSIDL_GET_SRV_TYPE_SUPPORT(ardupilot_msgs, srv, ArmMotors),
            "/ap/arm_motors");
        if (!(arm_motors_srv_init = (rc == RCL_RET_OK))) {
            uros_error("UROS: create arm_motors service... FAILED (%d)", (int16_t)rc);
            return false;
        }
        number_of_services++;
        uros_debug("UROS: create arm_motors service... OK");
    }

    {
        rcl_ret_t rc = rclc_service_init_default(
            &mode_switch_service,
            &node,
            ROSIDL_GET_SRV_TYPE_SUPPORT(ardupilot_msgs, srv, ModeSwitch),
            "/ap/mode_switch");
        if (!(mode_switch_srv_init = (rc == RCL_RET_OK))) {
            uros_error("UROS: create mode_switch service... FAILED (%d)", (int16_t)rc);
            return false;
        }
        number_of_services++;
        uros_debug("UROS: create mode_switch service... OK");
    }

#if AP_UROS_PARAM_SRV_ENABLED
    // create parameter server
    {
        hal.console->printf("UROS: create parameter server... ");

        // create parameter options
        // defaults:
        //    notify_changed_over_dds:      true
        //    max_params:                   4
        //    allow_undeclared_parameters:  false
        //    low_mem_mode:                 false
        const rclc_parameter_options_t options = {
            .notify_changed_over_dds = true,
            .max_params = 4,
            .allow_undeclared_parameters = true,
            .low_mem_mode = true
        };

        rcl_ret_t rc = rclc_parameter_server_init_with_option(
            &param_server, &node, &options);
        if ((param_srv_init = (rc == RCL_RET_OK))) {
            //! @note the parameter server requires 5 services + 1 publisher
            //  https://micro.ros.org/docs/tutorials/programming_rcl_rclc/parameters/ 
            number_of_publishers++;
            number_of_services += RCLC_EXECUTOR_PARAMETER_SERVER_HANDLES;
            hal.console->printf("OK\n");
        } else {
            hal.console->printf("FAIL: %d\n", (int16_t)rc);
        }
    }
#endif

    // create timer
    // uros_debug("UROS: create timer");
    UROS_CHECK(rclc_timer_init_default(
        &_timer,
        &support,
        RCL_MS_TO_NS(timer_timeout_ms),
        timer_callback_trampoline),
        "create timer");

    // number of entities
    uros_debug("UROS: number of publishers:  %d", number_of_publishers);
    uros_debug("UROS: number of subscribers: %d", number_of_subscribers);
    uros_debug("UROS: number of services:    %d", number_of_services);
    uint8_t number_of_handles =
        number_of_publishers + number_of_subscribers + number_of_services;

    // create executor
    // uros_debug("UROS: initialise executor");
    executor = rclc_executor_get_zero_initialized_executor();
    UROS_CHECK(rclc_executor_init(&executor, &support.context,
        number_of_handles, &allocator),
        "initialise executor");

    // uros_debug("UROS: add timer to executor");
  	UROS_CHECK(rclc_executor_add_timer(&executor, &_timer),
        "add timer to executor");

    // uros_debug("UROS: add subscriptions to executor");
    if (rx_joy_sub_init) {
        UROS_CHECK(rclc_executor_add_subscription_with_context(&executor, &rx_joy_subscriber,
            &rx_joy_msg, &AP_UROS_Client::on_joy_msg_trampoline, this, ON_NEW_DATA),
            "add subscription joy to executor");
    }
    if (rx_velocity_control_sub_init) {
        UROS_CHECK(rclc_executor_add_subscription_with_context(&executor, &rx_velocity_control_subscriber,
            &rx_velocity_control_msg, &AP_UROS_Client::on_velocity_control_msg_trampoline, this, ON_NEW_DATA),
            "add subscription velocity control to executor");
    }
    if (rx_dynamic_transforms_sub_init) {
        UROS_CHECK(rclc_executor_add_subscription_with_context(&executor, &rx_dynamic_transforms_subscriber,
            &rx_dynamic_transforms_msg, &AP_UROS_Client::on_tf_msg_trampoline, this, ON_NEW_DATA),
            "add subscription dynamic transforms to executor");
    }

    // uros_debug("UROS: add services to executor");
    if (arm_motors_srv_init) {
        UROS_CHECK(rclc_executor_add_service_with_context(&executor, &arm_motors_service,
            &arm_motors_req, &arm_motors_res,
            &AP_UROS_Client::arm_motors_callback_trampoline, this),
            "add service arm motors to executor");
    }
    if (mode_switch_srv_init) {
        UROS_CHECK(rclc_executor_add_service_with_context(&executor, &mode_switch_service,
            &mode_switch_req, &mode_switch_res,
            &AP_UROS_Client::mode_switch_callback_trampoline, this),
            "add service mode switch to executor");
    }

#if AP_UROS_PARAM_SRV_ENABLED
    // uros_debug("UROS: add parameter server to executor");
    if (param_srv_init) {
        UROS_CHECK(rclc_executor_add_parameter_server_with_context(&executor, &param_server,
            &AP_UROS_Client::on_parameter_changed_trampoline, this),
            "add parameter server to executor");
    }
#endif

    uros_debug("UROS: create complete");

    return true;
}

#if CONFIG_HAL_BOARD != HAL_BOARD_SITL && CONFIG_HAL_BOARD != HAL_BOARD_ESP32

extern "C" {
    int clock_gettime(clockid_t clockid, struct timespec *ts);
}

int clock_gettime(clockid_t clockid, struct timespec *ts)
{
    //! @todo the value of clockid is ignored here.
    //! A fallback mechanism is employed against the caller's choice of clock.
    uint64_t utc_usec;
    if (!AP::rtc().get_utc_usec(utc_usec)) {
        utc_usec = AP_HAL::micros64();
    }
    ts->tv_sec = utc_usec / 1000000ULL;
    ts->tv_nsec = (utc_usec % 1000000ULL) * 1000UL;
    return 0;
}


// Placeholder for fprintf() usage in:
//
// rosidl_runtime_c/src/string_functions.c 
// rosidl_runtime_c__String__fini(rosidl_runtime_c__String * str)
//
// rosidl_runtime_c/src/u16string_functions.c 
// rosidl_runtime_c__U16String__fini(rosidl_runtime_c__U16String * str)
//
#include <AP_Filesystem/posix_compat.h>

extern "C" {
    size_t __wrap_fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream);
}

size_t __wrap_fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream)
{
  // return dll_fwrite(ptr, size, nmemb, stream);
  return 0;
}


#endif // CONFIG_HAL_BOARD

#endif // AP_UROS_ENABLED


