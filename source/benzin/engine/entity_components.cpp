#include "benzin/config/bootstrap.hpp"
#include "benzin/engine/entity_components.hpp"

namespace benzin
{

    // Transform

    const DirectX::XMMATRIX& Transform::GetLocalToWorldMatrix() const
    {
        // Force update 'm_PreviousWorldMatrix'
        m_PrevLocalToWorldMatrix = m_LocalToWorldMatrix;

        if (m_IsDirty)
        {
            const DirectX::XMMATRIX rotation = DirectX::XMMatrixRotationX(m_Rotation.x) * DirectX::XMMatrixRotationY(m_Rotation.y) * DirectX::XMMatrixRotationZ(m_Rotation.z);
            const DirectX::XMMATRIX scaling = DirectX::XMMatrixScaling(m_Scale.x, m_Scale.y, m_Scale.z);
            const DirectX::XMMATRIX translation = DirectX::XMMatrixTranslation(m_Translation.x, m_Translation.y, m_Translation.z);

            m_LocalToWorldMatrix = scaling * rotation * translation;
            m_IsDirty = false;
        }

        return m_LocalToWorldMatrix;
    }

    const DirectX::XMMATRIX& Transform::GetPrevLocalToWorldMatrix() const
    {
        GetLocalToWorldMatrix();
        return m_PrevLocalToWorldMatrix;
    }

    void Transform::SetScale(const DirectX::XMFLOAT3& scale)
    {
        m_Scale = scale;
        m_IsDirty = true;
    }

    void Transform::SetRotation(const DirectX::XMFLOAT3& rotation)
    {
        m_Rotation = rotation;
        m_IsDirty = true;
    }

    void Transform::SetTranslation(const DirectX::XMFLOAT3& translation)
    {
        m_Translation = translation;
        m_IsDirty = true;
    }

}
