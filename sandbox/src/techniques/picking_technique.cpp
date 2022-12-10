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

    void PickingTechnique::SetPickableRenderItems(const std::span<InstancingAndCullingLayer::RenderItem*>& pickableRenderItems)
    {
        m_PickableRenderItems = pickableRenderItems;
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

        for (const auto& renderItem : m_PickableRenderItems)
        {
            const spieler::Vertex* vertices
            {
                reinterpret_cast<const spieler::Vertex*>(renderItem->MeshGeometry->GetVertices().data()) + renderItem->SubmeshGeometry->BaseVertexLocation
            };

            const uint32_t* indices
            {
                reinterpret_cast<const uint32_t*>(renderItem->MeshGeometry->GetIndices().data()) + renderItem->SubmeshGeometry->StartIndexLocation
            };

            float minInstanceDistance{ std::numeric_limits<float>::max() };
            uint32_t minInstanceIndex{ 0 };
            uint32_t minTriangleIndex{ 0 };

            for (size_t instanceIndex = 0; instanceIndex < renderItem->Instances.size(); ++instanceIndex)
            {
                if (!renderItem->Instances[instanceIndex].Visible)
                {
                    continue;
                }

                const DirectX::XMMATRIX toLocal{ DirectX::XMMatrixMultiply(m_ActiveCamera->GetInverseView(), renderItem->Instances[instanceIndex].Transform.GetInverseMatrix()) };

                const DirectX::XMVECTOR rayOrigin{ DirectX::XMVector3TransformCoord(viewRayOrigin, toLocal) };
                const DirectX::XMVECTOR rayDirection{ DirectX::XMVector3Normalize(DirectX::XMVector3TransformNormal(viewRayDirection, toLocal)) };

                float instanceDistance{ 0.0f };

                if (!renderItem->SubmeshGeometry->BoundingBox.Intersects(rayOrigin, rayDirection, instanceDistance))
                {
                    continue;
                }

                if (minInstanceDistance > instanceDistance)
                {
                    // Min Instance
                    minInstanceDistance = instanceDistance;
                    minInstanceIndex = static_cast<uint32_t>(instanceIndex);

                    float minTriangleDistance{ std::numeric_limits<float>::max() };

                    for (uint32_t triangleIndex = 0; triangleIndex < renderItem->SubmeshGeometry->IndexCount / 3; ++triangleIndex)
                    {
                        // Triangle
                        const DirectX::XMVECTOR a{ DirectX::XMLoadFloat3(&vertices[indices[triangleIndex * 3 + 0]].Position) };
                        const DirectX::XMVECTOR b{ DirectX::XMLoadFloat3(&vertices[indices[triangleIndex * 3 + 1]].Position) };
                        const DirectX::XMVECTOR c{ DirectX::XMLoadFloat3(&vertices[indices[triangleIndex * 3 + 2]].Position) };

                        float triangleDistance{ 0.0f };

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

                result.PickedRenderItem = renderItem;
                result.InstanceIndex = static_cast<uint32_t>(minInstanceIndex);
                result.TriangleIndex = minTriangleIndex;
            }
        }

        return result;
	}

} // namespace sandbox