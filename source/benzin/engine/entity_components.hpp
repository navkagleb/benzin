#pragma once

namespace benzin
{

    class Transform
    {
    public:
        const auto& GetScale() const { return m_Scale; }
        const auto& GetRotation() const { return m_Rotation; }
        const auto& GetTranslation() const { return m_Translation; }

        const DirectX::XMMATRIX& GetLocalToWorldMatrix() const;
        const DirectX::XMMATRIX& GetPrevLocalToWorldMatrix() const;

        void SetScale(const DirectX::XMFLOAT3& scale);
        void SetRotation(const DirectX::XMFLOAT3& rotation);
        void SetTranslation(const DirectX::XMFLOAT3& translation);

    private:
        DirectX::XMFLOAT3 m_Scale{ 1.0f, 1.0f, 1.0f };
        DirectX::XMFLOAT3 m_Rotation{ 0.0f, 0.0f, 0.0f };
        DirectX::XMFLOAT3 m_Translation{ 0.0f, 0.0f, 0.0f };

        mutable bool m_IsDirty = true;
        mutable DirectX::XMMATRIX m_LocalToWorldMatrix = DirectX::XMMatrixIdentity();
        mutable DirectX::XMMATRIX m_PrevLocalToWorldMatrix = DirectX::XMMatrixIdentity();
    };

    // TODO: Rename to MeshInstance?
    struct MeshComponent
    {
        entt::entity MeshHandle = g_InvalidEnum<entt::entity>;
    };

    using EntityUpdateCallback = std::function<void()>;

}
