#pragma once

#include "spieler/config/build_config.hpp"

#include <ctime>
#include <cmath>
#include <cstdint>

#if defined(SPIELER_DEBUG)
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

#include "spieler/config/win64_includes.hpp"
#include "spieler/config/dx12_includes.hpp"

#include "spieler/config/engine_config.hpp"

#include "spieler/core/class_modifiers.hpp"