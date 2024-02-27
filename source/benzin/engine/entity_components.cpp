#include "benzin/config/bootstrap.hpp"
#include "benzin/engine/entity_components.hpp"

#include <shaders/joint/structured_buffer_types.hpp>

#include "benzin/core/asserter.hpp"
#include "benzin/core/math.hpp"
#include "benzin/graphics/buffer.hpp"
#include "benzin/graphics/device.hpp"
#include "benzin/graphics/mapped_data.hpp"

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

    const Descriptor& TransformComponent::GetActiveTransformConstantBufferCBV(const Device& device) const
    {
        return m_TransformConstantBuffer->GetConstantBufferView(device.GetActiveFrameIndex());
    }

    void TransformComponent::UpdateMatricesIfNeeded()
    {
        if (!m_IsDirty)
        {
            return;
        }

        const DirectX::XMMATRIX rotation = DirectX::XMMatrixRotationX(m_Rotation.x) * DirectX::XMMatrixRotationY(m_Rotation.y) * DirectX::XMMatrixRotationZ(m_Rotation.z);
        const DirectX::XMMATRIX scaling = DirectX::XMMatrixScaling(m_Scale.x, m_Scale.y, m_Scale.z);
        const DirectX::XMMATRIX translation = DirectX::XMMatrixTranslation(m_Translation.x, m_Translation.y, m_Translation.z);
        const DirectX::XMMATRIX currentWorldMatrix = scaling * rotation * translation;

        m_PreviousWorldMatrix = DirectX::XMMatrixIsIdentity(m_PreviousWorldMatrix) ? currentWorldMatrix : m_WorldMatrix;
        m_WorldMatrix = currentWorldMatrix;
        m_WorldMatrixForNormals = GetMatrixForNormals(m_WorldMatrix);

        m_IsDirty = false;
    }

    void TransformComponent::CreateTransformConstantBuffer(Device& device, std::string_view debugName)
    {
        m_TransformConstantBuffer = CreateFrameDependentConstantBuffer<joint::MeshTransform>(device, debugName);
    }

    void TransformComponent::UpdateTransformConstantBuffer(const Device& device)
    {
        UpdateMatricesIfNeeded();

        // #TODO: Can skip writing if matrix is not updated

        const joint::MeshTransform transform
        {
            .Matrix = m_WorldMatrix,
            .PreviousMatrix = m_PreviousWorldMatrix,
            .MatrixForNormals = m_WorldMatrixForNormals,
        };

        MappedData transformConstantBuffer{ *m_TransformConstantBuffer };
        transformConstantBuffer.Write(transform, device.GetActiveFrameIndex());
    }

} // namespace benzin
