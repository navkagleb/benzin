#include <benzin/config/bootstrap.hpp>
#include <benzin/graphics/common.hpp>

namespace benzin
{

    uint32_t GetDxgiFormatSizeInBytes(DXGI_FORMAT dxgiFormat)
    {
        switch (dxgiFormat)
        {
        case DXGI_FORMAT_R32G32B32A32_FLOAT:
            return 4 * (32 / 8);
        case DXGI_FORMAT_R16G16B16A16_FLOAT:
        case DXGI_FORMAT_R16G16B16A16_UNORM:
            return 4 * (16 / 8);
        case DXGI_FORMAT_R8G8B8A8_UNORM:
        case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
            return 4 * (8 / 8);
        case DXGI_FORMAT_R32G32B32_FLOAT:
            return 3 * (32 / 8);
        case DXGI_FORMAT_R11G11B10_FLOAT:
            return 1 * ((11 + 11 + 10) / 8);
        case DXGI_FORMAT_R32G32_FLOAT:
            return 2 * (32 / 8);
        case DXGI_FORMAT_R16G16_FLOAT:
            return 2 * (16 / 8);
        case DXGI_FORMAT_R8G8_UNORM:
            return 2 * (8 / 8);
        case DXGI_FORMAT_R8_UINT:
        case DXGI_FORMAT_R8_UNORM:
            return 1 * (8 / 8);
        case DXGI_FORMAT_R16_FLOAT:
        case DXGI_FORMAT_R16_UINT:
        case DXGI_FORMAT_R16_UNORM:
            return 1 * (16 / 8);
        case DXGI_FORMAT_R32_FLOAT:
        case DXGI_FORMAT_R32_UINT:
        case DXGI_FORMAT_R32_TYPELESS:
            return 1 * (32 / 8);
        }

        BenzinAssert(false, "Unhandled DXGI_FORMAT: {}", magic_enum::enum_name(dxgiFormat));
        return 0;
    }

}
