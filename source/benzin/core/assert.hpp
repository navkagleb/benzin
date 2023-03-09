#pragma once

namespace benzin
{

#if defined(BENZIN_DEBUG)
    #define BENZIN_ASSERT(expression) assert(expression)
#else
    #define BENZIN_ASSERT(expession) expression
#endif

} // namespace benzin
