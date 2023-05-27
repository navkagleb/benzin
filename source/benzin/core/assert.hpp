#pragma once

namespace benzin
{

#if defined(BENZIN_DEBUG_BUILD)
    #define BENZIN_ASSERT(expression) assert(expression)
    #define BENZIN_HR_ASSERT(hrExpression) BENZIN_ASSERT(SUCCEEDED(hrExpression))
#else
    #define BENZIN_ASSERT(expression) ((void)(expression))
    #define BENZIN_HR_ASSERT(hrExpression) ((void)(hrExpression))
#endif

} // namespace benzin
