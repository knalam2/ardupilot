#pragma once

#include <AP_HAL/AP_HAL_Boards.h>

#ifndef HAL_GENERATOR_ENABLED
#define HAL_GENERATOR_ENABLED BOARD_FLASH_SIZE > 2048
#endif

#ifndef AP_GENERATOR_BACKEND_DEFAULT_ENABLED
#define AP_GENERATOR_BACKEND_DEFAULT_ENABLED HAL_GENERATOR_ENABLED
#endif

#ifndef AP_GENERATOR_IE_ENABLED
#define AP_GENERATOR_IE_ENABLED AP_GENERATOR_BACKEND_DEFAULT_ENABLED
#endif

#ifndef AP_GENERATOR_IE_2400_ENABLED
#define AP_GENERATOR_IE_2400_ENABLED AP_GENERATOR_IE_ENABLED
#endif

#ifndef AP_GENERATOR_IE_650_800_ENABLED
#define AP_GENERATOR_IE_650_800_ENABLED AP_GENERATOR_IE_ENABLED
#endif

#ifndef AP_GENERATOR_RICHENPOWER_ENABLED
#define AP_GENERATOR_RICHENPOWER_ENABLED AP_GENERATOR_BACKEND_DEFAULT_ENABLED
#endif
