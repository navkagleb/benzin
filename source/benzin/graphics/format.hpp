#pragma once

namespace benzin
{

    enum class GraphicsFormat : std::underlying_type_t<DXGI_FORMAT>
    {
        Unknown = DXGI_FORMAT_UNKNOWN,

        Rgba32Float = DXGI_FORMAT_R32G32B32A32_FLOAT,
        Rgba16Float = DXGI_FORMAT_R16G16B16A16_FLOAT,
        Rgba8Unorm = DXGI_FORMAT_R8G8B8A8_UNORM,

        Rgb32Float = DXGI_FORMAT_R32G32B32_FLOAT,
        Rg11B10Float = DXGI_FORMAT_R11G11B10_FLOAT, // Position-only format

        Rg16Float = DXGI_FORMAT_R16G16_FLOAT,
        Rg8Unorm = DXGI_FORMAT_R8G8_UNORM,

        D24Unorm_S8Uint = DXGI_FORMAT_D24_UNORM_S8_UINT,
        D24Unorm_X8Typeless = DXGI_FORMAT_R24_UNORM_X8_TYPELESS,

        Bc1Unorm = DXGI_FORMAT_BC1_UNORM,
        Bc3Unorm = DXGI_FORMAT_BC3_UNORM,
        Bc7Unorm = DXGI_FORMAT_BC7_UNORM,

        R16Uint = DXGI_FORMAT_R16_UINT,

        R32Float = DXGI_FORMAT_R32_FLOAT,
        R32Uint = DXGI_FORMAT_R32_UINT,
        R32Typeless = DXGI_FORMAT_R32_TYPELESS,
    };

    uint32_t GetFormatSizeInBytes(GraphicsFormat format);

} // namespace benzin
