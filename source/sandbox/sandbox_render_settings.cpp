#include "sandbox/bootstrap.hpp"
#include "sandbox/sandbox_render_settings.hpp"

#include <benzin/core/engine_math.hpp>

namespace sandbox
{

    DirectX::XMFLOAT3 GetSunDirection(const DeferredLightingSettings& settings)
    {
        const float pitch = settings.SunElevationInRadians;
        const float yaw = settings.SunAzimuthInRadians;

        auto sunDirection = benzin::GetDirectionFromPitchYaw(pitch, yaw); // sunDirection vector directed towards the sun
        sunDirection = DirectX::XMVector3Normalize(sunDirection);
        
        DirectX::XMFLOAT3 sunDirection3{};
        DirectX::XMStoreFloat3(&sunDirection3, sunDirection);

        return sunDirection3;
    }

}
