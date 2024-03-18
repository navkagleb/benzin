#include "benzin/config/bootstrap.hpp"
#include "benzin/engine/entity_components.hpp"

#include <shaders/joint/structured_buffer_types.hpp>

#include "benzin/core/asserter.hpp"
#include "benzin/core/math.hpp"
#include "benzin/graphics/buffer.hpp"
#include "benzin/graphics/device.hpp"

namespace benzin
{

    void TransformComponent::SetScale(const DirectX::XMFLOAT3& scale)
    {
        m_Scale = scale;
        m_IsDirty = true;
    }

    void TransformComponent::SetRotation(const DirectX::XMFLOAT3& rotation)
    {
        m_Rotation = rotation;
        m_IsDirty = true;
    }

    void TransformComponent::SetTranslation(const DirectX::XMFLOAT3& translation)
    {
        m_Translation = translation;
        m_IsDirty = true;
    }

    const DirectX::XMMATRIX& TransformComponent::GetWorldMatrix() const
    {
        BenzinAssert(!m_IsDirty);
        return m_WorldMatrix;
    }

    const Descriptor& TransformComponent::GetActiveTransformCbv() const
    {
        return m_TransformConstantBuffer->GetActiveCbv();
    }

    void TransformComponent::UpdateMatricesIfNeeded()
    {
        // Force update 'm_PreviousWorldMatrix'
        m_PreviousWorldMatrix = m_WorldMatrix;

        if (!m_IsDirty)
        {
            return;
        }

        const DirectX::XMMATRIX rotation = DirectX::XMMatrixRotationX(m_Rotation.x) * DirectX::XMMatrixRotationY(m_Rotation.y) * DirectX::XMMatrixRotationZ(m_Rotation.z);
        const DirectX::XMMATRIX scaling = DirectX::XMMatrixScaling(m_Scale.x, m_Scale.y, m_Scale.z);
        const DirectX::XMMATRIX translation = DirectX::XMMatrixTranslation(m_Translation.x, m_Translation.y, m_Translation.z);
        const DirectX::XMMATRIX currentWorldMatrix = scaling * rotation * translation;

        m_WorldMatrix = currentWorldMatrix;
        m_WorldMatrixForNormals = GetMatrixForNormals(m_WorldMatrix);

        m_IsDirty = false;
    }

    void TransformComponent::CreateTransformConstantBuffer(Device& device, std::string_view debugName)
    {
        MakeUniquePtr(m_TransformConstantBuffer, device, debugName);
    }

    void TransformComponent::UpdateTransformConstantBuffer()
    {
        UpdateMatricesIfNeeded();

        // #TODO: Can skip writing if matrix is not updated
        m_TransformConstantBuffer->UpdateConstants(joint::MeshTransform
        {
            .WorldMatrix = m_WorldMatrix,
            .PreviousWorldMatrix = m_PreviousWorldMatrix,
            .WorldMatrixForNormals = m_WorldMatrixForNormals,
        });
    }

} // namespace benzin
