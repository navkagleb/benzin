#pragma once

#include <cstdint>

#include <d3d12.h>

namespace Spieler
{

    enum ShaderVisibility : std::uint32_t
    {
        ShaderVisibility_All = D3D12_SHADER_VISIBILITY_ALL,
        ShaderVisibility_Vertex = D3D12_SHADER_VISIBILITY_VERTEX,
        ShaderVisibility_Hull = D3D12_SHADER_VISIBILITY_HULL,
        ShaderVisibility_Domain = D3D12_SHADER_VISIBILITY_DOMAIN,
        ShaderVisibility_Geometry = D3D12_SHADER_VISIBILITY_GEOMETRY,
        ShaderVisibility_Pixel = D3D12_SHADER_VISIBILITY_PIXEL,
    };

} // namespace Spieler