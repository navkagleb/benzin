#include "benzin/config/bootstrap.hpp"
#include "benzin/core/math.hpp"

namespace benzin
{

    // Ref: https://stackoverflow.com/questions/31952237/looking-for-a-constexpr-ceil-function/31957574

    static_assert(I32Floor(0.0f) == 0);
    static_assert(I32Floor(0.499999f) == 0);
    static_assert(I32Floor(0.5f) == 0);
    static_assert(I32Floor(0.999999f) == 0);
    static_assert(I32Floor(1.0f) == 1);
    static_assert(I32Floor(123.0f) == 123);
    static_assert(I32Floor(123.4f) == 123);

    static_assert(I32Floor(-0.499999f) == -1);
    static_assert(I32Floor(-0.5f) == -1);
    static_assert(I32Floor(-0.999999f) == -1);
    static_assert(I32Floor(-1.0f) == -1);
    static_assert(I32Floor(-123.0f) == -123);
    static_assert(I32Floor(-123.4f) == -124);

    static_assert(I32Ceil(0.0f) == 0);
    static_assert(I32Ceil(0.499999f) == 1);
    static_assert(I32Ceil(0.5f) == 1);
    static_assert(I32Ceil(0.999999f) == 1);
    static_assert(I32Ceil(1.0f) == 1);
    static_assert(I32Ceil(123.0f) == 123);
    static_assert(I32Ceil(123.4f) == 124);

    static_assert(I32Ceil(-0.499999f) == 0);
    static_assert(I32Ceil(-0.5f) == 0);
    static_assert(I32Ceil(-0.999999f) == 0);
    static_assert(I32Ceil(-1.0f) == -1);
    static_assert(I32Ceil(-123.0f) == -123);
    static_assert(I32Ceil(-123.4f) == -123);

}
