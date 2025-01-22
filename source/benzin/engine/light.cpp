#include "benzin/config/bootstrap.hpp"
#include "benzin/engine/light.hpp"

#include "benzin/core/engine_math.hpp"

namespace benzin
{

    // Light

    void Light::SetColor(const DirectX::XMFLOAT3& color)
    {
        m_Color = color;
    }

    void Light::SetIntensity(float intensity)
    {
        m_Intensity = intensity;
    }

    // SunLight

    void SunLight::SetAngularDiameterInRadians(float angleInRadians)
    {
        m_AngularDiameterInRadians = angleInRadians;
    }

    void SunLight::SetAzimuthInRadians(float angleInRadians)
    {
        m_AzimuthInRadians = angleInRadians;
    }

    void SunLight::SetElevationInRadians(float angleInRadians)
    {
        m_ElevationInRadians = angleInRadians;
    }

    DirectX::XMFLOAT3 SunLight::CalcToSunDirection() const
    {
        const float pitch = m_ElevationInRadians;
        const float yaw = m_AzimuthInRadians;

        auto sunDirection = benzin::GetDirectionFromPitchYaw(pitch, yaw); // sunDirection vector directed towards the sun
        sunDirection = DirectX::XMVector3Normalize(sunDirection);

        DirectX::XMFLOAT3 sunDirection3{};
        DirectX::XMStoreFloat3(&sunDirection3, sunDirection);

        return sunDirection3;
    }

    // SphericalLight

    void SphericalLight::SetPosition(const DirectX::XMFLOAT3& position)
    {
        m_Transform.SetTranslation(position);
    }

    void SphericalLight::SetRange(float range)
    {
        m_Range = range;

        m_Attenuation.y = 4.5f / range;
        m_Attenuation.z = 75.0f / (range * range);
    }

    void SphericalLight::SetRadius(float radius)
    {
        const float diameter = radius * 2.0f;
        m_Transform.SetScale({ diameter, diameter, diameter });
    }

}
