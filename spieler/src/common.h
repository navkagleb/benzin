#pragma once

#include <cassert>

#include <wrl.h>

namespace Spieler
{

#if defined(DEBUG) || defined(_DEBUG)
    #define SPIELER_DEBUG
#endif

#if defined(SPIELER_DEBUG)
    #define SPIELER_ASSERT(expression) assert(expression)
#endif

    template <typename T>
    using ComPtr = Microsoft::WRL::ComPtr<T>;

} // namespace Spieler