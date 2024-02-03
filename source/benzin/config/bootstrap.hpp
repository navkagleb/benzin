#pragma once

#include "benzin/config/win64_includes.hpp" // Include win64 first of all because other dependencies uses #include <Windows.h>

#include "benzin/config/build_config.hpp"
#include "benzin/config/d3d12_includes.hpp"
#include "benzin/config/std_includes.hpp"
#include "benzin/config/third_party_includes.hpp"

#include "benzin/utility/benzin_defines.hpp"
#include "benzin/utility/file_utils.hpp"
#include "benzin/utility/string_utils.hpp"

#include "benzin/core/assert.hpp"
#include "benzin/core/common.hpp"
#include "benzin/core/logger.hpp"
#include "benzin/core/scoped_log_timer.hpp"

// Global configs
#include "benzin/config/engine_config.hpp"
#include "benzin/config/graphics_config.hpp"

#include <shaders/joint/constant_buffer_types.hpp>
#include <shaders/joint/enum_types.hpp>
#include <shaders/joint/root_constants.hpp>
#include <shaders/joint/structured_buffer_types.hpp>
