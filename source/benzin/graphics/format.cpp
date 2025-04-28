#include "benzin/config/bootstrap.hpp"
#include "benzin/graphics/format.hpp"

namespace benzin
{

    uint32_t GetFormatSizeInBytes(GraphicsFormat format)
    {
        switch (format)
        {
            using enum GraphicsFormat;

            case Rgba32Float: return 4 * (32 / 8);
            case Rgba16Float:
            case Rgba16Unorm: return 4 * (16 / 8);
            case Rgba8Unorm:
            case Rgba8Unorm_Srgb: return 4 * (8 / 8);

            case Rgb32Float: return 3 * (32 / 8);
            case Rg11B10Float: return 1 * ((11 + 11 + 10) / 8);

            case Rg32Float: return 2 * (32 / 8);
            case Rg16Float: return 2 * (16 / 8);
            case Rg8Unorm: return 2 * (8 / 8);

            case D24Unorm_S8Uint:
            case D24Unorm_X8Typeless: return 1 * (32 / 8);

            case R8Uint:
            case R8Unorm: return 1 * (8 / 8);

            case R16Float:
            case R16Uint:
            case R16Unorm: return 1 * (16 / 8);

            case R32Float:
            case R32Uint:
            case R32Typeless: return 1 * (32 / 8);
        }

        BenzinAssert(false, "Not supported GraphicsFormat: {}", magic_enum::enum_integer(format));
        return 0;
    }

}
