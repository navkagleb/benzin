#pragma once

#include "benzin/config/win64_includes.hpp" // Include win64 first of all because other dependencies uses #include <Windows.h>

#include "benzin/config/build_config.hpp"
#include "benzin/config/d3d12_includes.hpp"
#include "benzin/config/std_includes.hpp"
#include "benzin/config/third_party_includes.hpp"

#include "benzin/utility/benzin_defines.hpp"
#include "benzin/utility/file_utils.hpp"
#include "benzin/utility/string_utils.hpp"
#include "benzin/utility/time_utils.hpp"

#include "benzin/core/common.hpp"
#include "benzin/core/scoped_timer.hpp"

// Global configs
#include "benzin/config/engine_config.hpp"
#include "benzin/config/graphics_config.hpp"
