#pragma once

#include <ctime>
#include <cmath>
#include <cstdint>

#if defined(BENZIN_DEBUG)
    #include <cassert>
#endif

#include <memory>
#include <functional>
#include <concepts>
#include <format>
#include <iomanip>
#include <iostream>
#include <chrono>
#include <random>
#include <utility>
#include <numbers>
#include <numeric>
#include <type_traits>

#include <string>
#include <sstream>
#include <fstream>
#include <array>
#include <vector>
#include <map>
#include <unordered_map>
#include <variant>
#include <any> // TODO: Remove any
#include <bitset>
#include <span>

#include "benzin/config/win64_includes.hpp"
#include "benzin/config/d3d12_includes.hpp"

#include "benzin/config/engine_config.hpp"

#include "benzin/utility/class_modifiers.hpp"

#include "benzin/core/assert.hpp"
#include "benzin/core/logger.hpp"
