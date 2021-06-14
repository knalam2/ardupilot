#pragma once

#if CONFIG_HAL_BOARD == HAL_BOARD_SITL
#include <AP_Math/AP_Math.h>
#include <AC_PrecLand/AC_PrecLand_Backend.h>
#include <AP_IRLock/AP_IRLock_SITL_Gazebo.h>

/*
 * AC_PrecLand_SITL_Gazebo - implements precision landing using target
 * vectors provided Gazebo via a network socket
 */

class AC_PrecLand_SITL_Gazebo : public AC_PrecLand_Backend
{
public:

    // Constructor
    AC_PrecLand_SITL_Gazebo(const AC_PrecLand& frontend, AC_PrecLand::precland_state& state);

    // perform any required initialisation of backend
    void init() override;

    // retrieve updates from sensor
    void update() override;

private:
    AP_IRLock_SITL_Gazebo irlock;

};

#endif
