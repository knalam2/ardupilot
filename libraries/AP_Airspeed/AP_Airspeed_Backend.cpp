/*
   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/*
  backend driver class for airspeed
 */

#include "AP_Airspeed_config.h"

#if AP_AIRSPEED_ENABLED

#include <AP_Common/AP_Common.h>
#include <AP_HAL/AP_HAL.h>
#include "AP_Airspeed.h"
#include "AP_Airspeed_Backend.h"

extern const AP_HAL::HAL &hal;

AP_Airspeed_Backend::AP_Airspeed_Backend(AP_Airspeed &_frontend, AP_Airspeed::airspeed_state &_state, AP_Airspeed_Params &_params) :
    frontend(_frontend),
    state(_state),
    params(_params)
{
}

AP_Airspeed_Backend::~AP_Airspeed_Backend(void)
{
}
 

int8_t AP_Airspeed_Backend::get_pin(void) const
{
#ifndef HAL_BUILD_AP_PERIPH
    return params.pin;
#else
    return 0;
#endif
}

float AP_Airspeed_Backend::get_psi_range(void) const
{
    return params.psi_range;
}

uint8_t AP_Airspeed_Backend::get_bus(void) const
{
    return params.bus;
}

bool AP_Airspeed_Backend::bus_is_configured(void) const
{
    return params.bus.configured();
}

void AP_Airspeed_Backend::set_bus_id(uint32_t id)
{
    params.bus_id.set_and_save(int32_t(id));
}

#endif  // AP_AIRSPEED_ENABLED
