#pragma once

#include <variant>

#include <DirectXMath.h>

namespace Spieler
{

    struct DirectionalLightConstants
    {
        alignas(16) DirectX::XMFLOAT3 Strength{};
        alignas(16) DirectX::XMFLOAT3 Direction{};
    };

    struct PointLightConstants
    {
        DirectX::XMFLOAT3   Strength{};
        float               FalloffStart{ 0.0f };
        DirectX::XMFLOAT3   Position{};
        float               FalloffEnd{ 0.0f };
    };

    struct SpotLightConstants
    {
        DirectX::XMFLOAT3   Strength{};
        float               FalloffStart{ 0.0f };
        DirectX::XMFLOAT3   Position{};
        float               FalloffEnd{ 0.0f };
        DirectX::XMFLOAT3   Direction{};
        float               SpotPower{ 0.0f };
    };

    struct LightConstants
    {
        DirectX::XMFLOAT3 Strength{ 1.0f, 1.0f, 1.0f };
        float FalloffStart{ 0.0f };
        DirectX::XMFLOAT3 Direction{ 0.0f, 0.0f, 0.0f };
        float FalloffEnd{ 0.0f };
        DirectX::XMFLOAT3 Position{ 0.0f, 0.0f, 0.0f };
        float SpotPower{ 0.0f };
    };

} // namespace Spieler