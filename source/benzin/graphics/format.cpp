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

            case RGBA32Float: return 4 * (32 / 8);
            case RGBA16Float: return 4 * (16 / 8);
            case RGBA8Unorm: return 4 * (8 / 8);

            case RGB32Float: return 3 * (32 / 8);

            case RG16Float: return 2 * (16 / 8);
            case RG8Unorm: return 2 * (8 / 8);

            case D24Unorm_S8Uint:
            case D24Unorm_X8Typeless: return 1 * (32 / 8);

            case R16Uint: return 1 * (16 / 8);

            case R32Uint:
            case R32Typeless: return 1 * (32 / 8);

            default:
            {
                BenzinEnsure(false);
            };
        }

        std::unreachable();
    }

} // namespace benzin
