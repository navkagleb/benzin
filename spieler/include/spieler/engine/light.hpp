#pragma once

namespace spieler
{

    struct LightConstants
    {
        DirectX::XMFLOAT3 Strength{ 1.0f, 1.0f, 1.0f };
        float FalloffStart{ 0.0f };
        DirectX::XMFLOAT3 Direction{ 0.0f, 0.0f, 0.0f };
        float FalloffEnd{ 0.0f };
        DirectX::XMFLOAT3 Position{ 0.0f, 0.0f, 0.0f };
        float SpotPower{ 0.0f };
    };

} // namespace spieler
