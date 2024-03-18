#include "benzin/config/bootstrap.hpp"
#include "benzin/graphics/format.hpp"

#include "benzin/core/asserter.hpp"

namespace benzin
{

    uint32_t GetFormatSizeInBytes(GraphicsFormat format)
    {
        switch (format)
        {
            using enum GraphicsFormat;

            case Rgba32Float: return 4 * (32 / 8);
            case Rgba16Float: return 4 * (16 / 8);
            case Rgba8Unorm: return 4 * (8 / 8);

            case Rgb32Float: return 3 * (32 / 8);
            case Rg11B10Float: return 1 * ((11 + 11 + 10) / 8);

            case Rg16Float: return 2 * (16 / 8);
            case Rg8Unorm: return 2 * (8 / 8);

            case D24Unorm_S8Uint:
            case D24Unorm_X8Typeless: return 1 * (32 / 8);

            case R16Uint: return 1 * (16 / 8);

            case R32Float:
            case R32Uint:
            case R32Typeless: return 1 * (32 / 8);

            default: BenzinEnsure(false);
        }

        std::unreachable();
    }

} // namespace benzin
