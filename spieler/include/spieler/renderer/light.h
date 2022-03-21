#pragma once

#include <DirectXMath.h>

namespace Spieler
{

    struct Light
    {
        DirectX::XMFLOAT3 Strength{};
    };

    struct DirectionalLight : Light
    {
        DirectX::XMFLOAT3 Direction{};
    };

    struct PointLight : Light
    {
        float               FalloffStart{ 0.0f };
        float               FalloffEnd{ 0.0f };
        DirectX::XMFLOAT3   Position{};
    };

    struct SpotLight : PointLight, DirectionalLight
    {
        float SpotPower{ 0.0f };
    };

} // namespace Spieler