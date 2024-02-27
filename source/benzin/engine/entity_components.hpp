#pragma once

namespace benzin
{

    class Buffer;
    class Descriptor;
    class Device;

    struct MeshInstanceComponent
    {
        uint32_t MeshUnionIndex = g_InvalidIndex<uint32_t>;
        std::optional<IndexRangeU32> MeshInstanceRange;
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

        const DirectX::XMMATRIX& GetWorldMatrix() const;

        const Descriptor& GetActiveTransformConstantBufferCBV(const Device& device) const;

    private:
        void UpdateMatricesIfNeeded();

        void CreateTransformConstantBuffer(Device& device, std::string_view debugName);
        void UpdateTransformConstantBuffer(const Device& device);

    private:
        DirectX::XMFLOAT3 m_Scale{ 1.0f, 1.0f, 1.0f };
        DirectX::XMFLOAT3 m_Rotation{ 0.0f, 0.0f, 0.0f };
        DirectX::XMFLOAT3 m_Translation{ 0.0f, 0.0f, 0.0f };

        bool m_IsDirty = true;
        DirectX::XMMATRIX m_WorldMatrix = DirectX::XMMatrixIdentity();
        DirectX::XMMATRIX m_PreviousWorldMatrix = DirectX::XMMatrixIdentity();
        DirectX::XMMATRIX m_WorldMatrixForNormals = DirectX::XMMatrixIdentity();

        std::unique_ptr<Buffer> m_TransformConstantBuffer;
    };

    struct UpdateComponent
    {
        std::function<void(entt::registry&, entt::entity, std::chrono::microseconds)> Callback;
    };

    struct PointLightComponent
    {
        DirectX::XMFLOAT3 Color;
        float Intensity;
        float Range;

        float GeometryRadius;
    };

} // namespace benzin
