#include "bootstrap.hpp"

#include "picking_technique.hpp"

namespace sandbox
{

    PickingTechnique::PickingTechnique(spieler::Window& window)
        : m_Window{ window }
    {}

    void PickingTechnique::SetActiveCamera(const spieler::Camera* activeCamera)
    {
        m_ActiveCamera = activeCamera;
    }

    void PickingTechnique::SetPickableEntities(const std::span<const spieler::Entity*>& pickableEntities)
    {
        m_PickableEntities = pickableEntities;
    }

	PickingTechnique::Result PickingTechnique::PickTriangle(float mouseX, float mouseY)
	{
        SPIELER_ASSERT(m_ActiveCamera);

        Result result;

        // In View Space
        const DirectX::XMVECTOR viewRayOrigin{ 0.0f, 0.0f, 0.0f, 1.0f };
        const DirectX::XMVECTOR viewRayDirection
        {
            (2.0f * mouseX / static_cast<float>(m_Window.GetWidth()) - 1.0f) / DirectX::XMVectorGetByIndex(m_ActiveCamera->GetProjection().r[0], 0),
            (-2.0f * mouseY / static_cast<float>(m_Window.GetHeight()) + 1.0f) / DirectX::XMVectorGetByIndex(m_ActiveCamera->GetProjection().r[1], 1),
            1.0f,
            0.0f
        };

        float minDistance{ std::numeric_limits<float>::max() };

        for (const auto& entity : m_PickableEntities)
        {
            const auto& meshComponent = entity->GetComponent<spieler::MeshComponent>();
            const auto& instancesComponent = entity->GetComponent<spieler::InstancesComponent>();

            const auto* vertices = reinterpret_cast<const spieler::Vertex*>(meshComponent.Mesh->GetVertices().data()) + meshComponent.SubMesh->BaseVertexLocation;
            const auto* indices = reinterpret_cast<const uint32_t*>(meshComponent.Mesh->GetIndices().data()) + meshComponent.SubMesh->StartIndexLocation;

            float minInstanceDistance = std::numeric_limits<float>::max();
            uint32_t minInstanceIndex = 0;
            uint32_t minTriangleIndex = 0;

            for (size_t instanceIndex = 0; instanceIndex < instancesComponent.Instances.size(); ++instanceIndex)
            {
                const auto& instanceComponent = instancesComponent.Instances[instanceIndex];

                if (!instanceComponent.IsVisible)
                {
                    continue;
                }

                const DirectX::XMMATRIX toLocal = DirectX::XMMatrixMultiply(m_ActiveCamera->GetInverseView(), instanceComponent.Transform.GetInverseMatrix());

                const DirectX::XMVECTOR rayOrigin = DirectX::XMVector3TransformCoord(viewRayOrigin, toLocal);
                const DirectX::XMVECTOR rayDirection = DirectX::XMVector3Normalize(DirectX::XMVector3TransformNormal(viewRayDirection, toLocal));

                float instanceDistance = 0.0f;

                if (!meshComponent.SubMesh->BoundingBox.Intersects(rayOrigin, rayDirection, instanceDistance))
                {
                    continue;
                }

                if (minInstanceDistance > instanceDistance)
                {
                    // Min Instance
                    minInstanceDistance = instanceDistance;
                    minInstanceIndex = instanceIndex;

                    float minTriangleDistance = std::numeric_limits<float>::max();

                    for (uint32_t triangleIndex = 0; triangleIndex < meshComponent.SubMesh->IndexCount / 3; ++triangleIndex)
                    {
                        // Triangle
                        const DirectX::XMVECTOR a = DirectX::XMLoadFloat3(&vertices[indices[triangleIndex * 3 + 0]].Position);
                        const DirectX::XMVECTOR b = DirectX::XMLoadFloat3(&vertices[indices[triangleIndex * 3 + 1]].Position);
                        const DirectX::XMVECTOR c = DirectX::XMLoadFloat3(&vertices[indices[triangleIndex * 3 + 2]].Position);

                        float triangleDistance = 0.0f;

                        if (DirectX::TriangleTests::Intersects(rayOrigin, rayDirection, a, b, c, triangleDistance))
                        {
                            if (minTriangleDistance > triangleDistance)
                            {
                                // Min Instance Triangle
                                minTriangleDistance = triangleDistance;
                                minTriangleIndex = triangleIndex;
                            }
                        }
                    }
                }
            }

            if (minDistance > minInstanceDistance)
            {
                // Min RenderItem
                minDistance = minInstanceDistance;

                result.PickedEntity = entity;
                result.InstanceIndex = minInstanceIndex;
                result.TriangleIndex = minTriangleIndex;
            }
        }

        return result;
	}

} // namespace sandbox