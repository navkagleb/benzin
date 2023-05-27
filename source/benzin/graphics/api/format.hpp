#pragma once

namespace benzin
{

    enum class GraphicsFormat : std::underlying_type_t<DXGI_FORMAT>
    {
        Unknown = DXGI_FORMAT_UNKNOWN,

        RGBA32Float = DXGI_FORMAT_R32G32B32A32_FLOAT,
        RGBA16Float = DXGI_FORMAT_R16G16B16A16_FLOAT,
        RGBA8Unorm = DXGI_FORMAT_R8G8B8A8_UNORM,

        RGB32Float = DXGI_FORMAT_R32G32B32_FLOAT,

        D24Unorm_S8Uint = DXGI_FORMAT_D24_UNORM_S8_UINT,
        D24Unorm_X8Typeless = DXGI_FORMAT_R24_UNORM_X8_TYPELESS,

        BC1Unorm = DXGI_FORMAT_BC1_UNORM,
        BC3Unorm = DXGI_FORMAT_BC3_UNORM,
        BC7Unorm = DXGI_FORMAT_BC7_UNORM,
    };

    uint32_t GetFormatSizeInBytes(GraphicsFormat format);

} // namespace benzin
