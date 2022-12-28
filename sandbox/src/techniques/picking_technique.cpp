#include "bootstrap.hpp"

#include "picking_technique.hpp"

namespace sandbox
{

    PickingTechnique::PickingTechnique(const spieler::Window& window)
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

	PickingTechnique::Result PickingTechnique::PickTriangle(float mouseX, float mouseY) const
	{
        SPIELER_ASSERT(m_ActiveCamera);

        const Ray rayInViewSpace = GetRayInViewSpace(mouseX, mouseY);

        Result result;
        float minDistance = std::numeric_limits<float>::max();

        for (const auto& entity : m_PickableEntities)
        {
            const auto& meshComponent = entity->GetComponent<spieler::MeshComponent>();
            const auto& instancesComponent = entity->GetComponent<spieler::InstancesComponent>();
            const auto* collisionComponent = entity->GetPtrComponent<spieler::CollisionComponent>();

            const auto [instanceDistance, instanceIndex] = PickEntityInstance(rayInViewSpace, instancesComponent, collisionComponent);

            SPIELER_INFO("!! {} {} | {}", instanceIndex, instanceDistance, instancesComponent.Instances.size());

            if (minDistance > instanceDistance)
            {
                minDistance = instanceDistance;

                result.PickedEntity = entity;
                result.InstanceIndex = instanceIndex;
                result.TriangleIndex = PickEntityInstanceTriangle(
                    GetRayInLocalSpace(rayInViewSpace, instancesComponent.Instances[instanceIndex]),
                    meshComponent
                );

                SPIELER_INFO("  -- {} {} {} -> {}", (const void*)entity, instanceIndex, result.TriangleIndex, minDistance);
            }
        }

        SPIELER_INFO("\n");

        return result;
	}

    PickingTechnique::Ray PickingTechnique::GetRayInViewSpace(float mouseX, float mouseY) const
    {
        const float ndcX = 2.0f * mouseX / static_cast<float>(m_Window.GetWidth()) - 1.0f;
        const float ndcY = -2.0f * mouseY / static_cast<float>(m_Window.GetHeight()) + 1.0f;

        const float viewX = ndcX / DirectX::XMVectorGetByIndex(m_ActiveCamera->GetProjection()->GetMatrix().r[0], 0);
        const float viewY = ndcY / DirectX::XMVectorGetByIndex(m_ActiveCamera->GetProjection()->GetMatrix().r[1], 1);
        const float viewZ = 1.0f;

        return Ray
        {
            .Origin{ 0.0f, 0.0f, 0.0f, 1.0f },
            .Direction{ viewX, viewY, viewZ, 0.0f }
        };
    }

    PickingTechnique::Ray PickingTechnique::GetRayInLocalSpace(const Ray& rayInViewSpace, const spieler::InstanceComponent& instanceComponent) const
    {
        const DirectX::XMMATRIX toLocalSpaceTransform = DirectX::XMMatrixMultiply(
            m_ActiveCamera->GetInverseViewMatrix(),
            instanceComponent.Transform.GetInverseMatrix()
        );

        return Ray
        {
            .Origin{ DirectX::XMVector3TransformCoord(rayInViewSpace.Origin, toLocalSpaceTransform) },
            .Direction{ DirectX::XMVector3Normalize(DirectX::XMVector3TransformNormal(rayInViewSpace.Direction, toLocalSpaceTransform)) }
        };
    }

    std::pair<float, uint32_t> PickingTechnique::PickEntityInstance(
        const Ray& rayInViewSpace,
        const spieler::InstancesComponent& instancesComponent,
        const spieler::CollisionComponent* collisionComponent
    ) const
    {
        if (!collisionComponent)
        {
            SPIELER_WARNING("Entity doesn't have CollisionComponent");
            return { std::numeric_limits<float>::max(), std::numeric_limits<uint32_t>::max() };
        }

        float minInstanceDistance = std::numeric_limits<float>::max();
        uint32_t minInstanceIndex = 0;

        for (size_t instanceIndex = 0; instanceIndex < instancesComponent.Instances.size(); ++instanceIndex)
        {
            const auto& instanceComponent = instancesComponent.Instances[instanceIndex];

            if (!instanceComponent.IsVisible)
            {
                continue;
            }

            const Ray rayInLocalSpace = GetRayInLocalSpace(rayInViewSpace, instanceComponent);
            const std::optional<float> instanceDistance = collisionComponent->HitRay(rayInLocalSpace.Origin, rayInLocalSpace.Direction);

            if (!instanceDistance)
            {
                continue;
            }

            if (minInstanceDistance > instanceDistance)
            {
                minInstanceDistance = *instanceDistance;
                minInstanceIndex = static_cast<uint32_t>(instanceIndex);
            }

            SPIELER_INFO("    Distance: {}, MinDistance: {}", *instanceDistance, minInstanceDistance);
        }

        return { minInstanceDistance, minInstanceIndex };
    }

    uint32_t PickingTechnique::PickEntityInstanceTriangle(const Ray& rayInLocalSpace, const spieler::MeshComponent& meshComponent) const
    {
        const auto* vertices = reinterpret_cast<const spieler::Vertex*>(meshComponent.Mesh->GetVertices().data()) + meshComponent.SubMesh->BaseVertexLocation;
        const auto* indices = reinterpret_cast<const uint32_t*>(meshComponent.Mesh->GetIndices().data()) + meshComponent.SubMesh->StartIndexLocation;

        float minTriangleDistance = std::numeric_limits<float>::max();
        uint32_t minTriangleIndex = 0;

        for (uint32_t triangleIndex = 0; triangleIndex < meshComponent.SubMesh->IndexCount / 3; ++triangleIndex)
        {
            // Triangle
            const DirectX::XMVECTOR a = DirectX::XMLoadFloat3(&vertices[indices[triangleIndex * 3 + 0]].Position);
            const DirectX::XMVECTOR b = DirectX::XMLoadFloat3(&vertices[indices[triangleIndex * 3 + 1]].Position);
            const DirectX::XMVECTOR c = DirectX::XMLoadFloat3(&vertices[indices[triangleIndex * 3 + 2]].Position);

            float triangleDistance = 0.0f;

            if (DirectX::TriangleTests::Intersects(rayInLocalSpace.Origin, rayInLocalSpace.Direction, a, b, c, triangleDistance))
            {
                if (minTriangleDistance > triangleDistance)
                {
                    minTriangleDistance = triangleDistance;
                    minTriangleIndex = triangleIndex;

                    SPIELER_WARNING("MinTriangleDistance: {}", minTriangleDistance);
                }
            }
        }

        return minTriangleIndex;
    }

} // namespace sandbox