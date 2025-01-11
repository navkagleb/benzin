#pragma once

namespace joint
{

    struct MeshTransform;

} // namespace joint

namespace benzin
{

    class Descriptor;
    class Device;
    class TickTimer;

    template <typename>
    class ConstantBuffer;

    struct MeshComponent
    {
        entt::entity MeshHandle = g_InvalidEnum<entt::entity>;
        std::optional<IndexRange32> MeshInstanceRange;
    };

    class TransformComponent
    {
    public:
        friend class Scene;

    public:
        const auto& GetScale() const { return m_Scale; }
        void SetScale(const DirectX::XMFLOAT3& scale);

        const auto& GetRotation() const { return m_Rotation; }
        void SetRotation(const DirectX::XMFLOAT3& rotation);

        const auto& GetTranslation() const { return m_Translation; }
        void SetTranslation(const DirectX::XMFLOAT3& translation);

        const DirectX::XMMATRIX& GetLocalToWorldMatrix() const;

        const Descriptor& GetActiveTransformCbv() const;

        void UpdateMatricesIfNeeded();

    private:
        void CreateTransformConstantBuffer(Device& device, std::string_view debugName);
        void UpdateTransformConstantBuffer();

    private:
        DirectX::XMFLOAT3 m_Scale{ 1.0f, 1.0f, 1.0f };
        DirectX::XMFLOAT3 m_Rotation{ 0.0f, 0.0f, 0.0f };
        DirectX::XMFLOAT3 m_Translation{ 0.0f, 0.0f, 0.0f };

        bool m_IsDirty = true;
        DirectX::XMMATRIX m_LocalToWorldMatrix = DirectX::XMMatrixIdentity();
        DirectX::XMMATRIX m_PrevLocalToWorldMatrix = DirectX::XMMatrixIdentity();

        std::unique_ptr<ConstantBuffer<joint::MeshTransform>> m_TransformConstantBuffer;
    };

    struct UpdateComponent
    {
        using FrameUpdateCallback = std::function<void(entt::registry&, entt::entity)>;
        FrameUpdateCallback Callback;
    };

    struct PointLightComponent
    {
        DirectX::XMFLOAT3 Color;
        float Intensity;
        float Range;

        float GeometryRadius;
    };

}
