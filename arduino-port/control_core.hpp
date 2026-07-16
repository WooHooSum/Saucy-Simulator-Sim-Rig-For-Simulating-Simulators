#pragma once

#include "shared_state.hpp"
#include "src/wheel/config.hpp"

bool start_control_task(RuntimeShared& shared, const wheel::ConfigRecord& boot_config,
                        bool boot_config_from_nvs);
