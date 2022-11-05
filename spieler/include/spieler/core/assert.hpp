#pragma once

namespace spieler
{

#if defined(SPIELER_DEBUG)
    #define SPIELER_ASSERT(expression) assert(expression)
#else
    #define SPIELER_ASSERT(expession) expression
#endif

} // namespace spieler
