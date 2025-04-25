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

    class MeshInstanceComponent
    {
    public:
        friend class Scene;

        explicit MeshInstanceComponent(entt::entity meshHandle)
            : m_MeshHandle{ meshHandle }
        {}

        auto GetMeshHandle() const { return m_MeshHandle; }
        auto GetEntityTransformIndex() const { return m_EntityTransformIndex; }

    private:
        entt::entity m_MeshHandle = g_BadEnum<entt::entity>;
        uint32_t m_EntityTransformIndex = g_Bad32;
    };

    using EntityUpdateCallback = std::function<void()>;

}
