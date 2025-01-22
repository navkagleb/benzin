#pragma once

#include "entity_components.hpp"

namespace benzin
{

    class Light
    {
    public:
        const auto& GetColor() const { return m_Color; }
        auto GetIntensity() const { return m_Intensity; }

        void SetColor(const DirectX::XMFLOAT3& color);
        void SetIntensity(float intensity);

    protected:
        DirectX::XMFLOAT3 m_Color{ 1.0f, 1.0f, 1.0f };
        float m_Intensity = 1.0f;
    };

    class SunLight : public Light
    {
    public:
        auto GetAngularDiameterInRadians() const { return m_AngularDiameterInRadians; }
        auto GetAzimuthInRadians() const { return m_AzimuthInRadians; }
        auto GetElevationInRadians() const { return m_ElevationInRadians; }

        void SetAngularDiameterInRadians(float angleInRadians);
        void SetAzimuthInRadians(float angleInRadians);
        void SetElevationInRadians(float angleInRadians);

        DirectX::XMFLOAT3 CalcToSunDirection() const;

    private:
        float m_AngularDiameterInRadians = DirectX::XMConvertToRadians(0.5f); // [0.01f, 5.0f]
        float m_AzimuthInRadians = DirectX::XMConvertToRadians(0.0f); // [-180.0f, 180.0f]
        float m_ElevationInRadians = DirectX::XMConvertToRadians(45.0f); // [0.0f, 180.0]
    };

    class SphericalLight : public Light
    {
    public:
        // TODO: Ugly interface
        const auto& GetTransform() const { return m_Transform; }
        const auto& GetPosition() const { return m_Transform.GetTranslation(); }
        const auto& GetAttenuation() const { return m_Attenuation; }
        auto GetRange() const { return m_Range; }
        auto GetRadius() const { return m_Transform.GetScale().x * 0.5f; }
        auto IsEnabled() const { return m_IsEnabled; }

        void SetPosition(const DirectX::XMFLOAT3& position);
        void SetRange(float range);
        void SetRadius(float radius);
        void SetEnabled(bool isEnabled) { m_IsEnabled = isEnabled; }

    private:
        Transform m_Transform;

        DirectX::XMFLOAT3 m_Attenuation{ 1.0f, 0.0f, 0.0f };
        float m_Range = 0.0;
        bool m_IsEnabled = true;
    };

}
