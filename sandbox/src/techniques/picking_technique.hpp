#pragma once

#include <spieler/system/window.hpp>
#include <spieler/engine/mesh.hpp>
#include <spieler/engine/camera.hpp>

namespace sandbox
{

	class PickingTechnique
	{
	public:
		struct Result
		{
			const spieler::Entity* PickedEntity{ nullptr };
			uint32_t InstanceIndex{ 0 };
			uint32_t TriangleIndex{ 0 };
		};

	private:
		struct Ray
		{
			DirectX::XMVECTOR Origin{};
			DirectX::XMVECTOR Direction{};
		};

	public:
		explicit PickingTechnique(const spieler::Window& window);

	public:
		void SetActiveCamera(const spieler::Camera* activeCamera);
		void SetPickableEntities(const std::span<const spieler::Entity*>& pickableEntities);

	public:
		Result PickTriangle(float mouseX, float mouseY) const;

	private:
		Ray GetRayInViewSpace(float mouseX, float mouseY) const;
		Ray GetRayInLocalSpace(const Ray& rayInViewSpace, const spieler::InstanceComponent& instanceComponent) const;

		std::pair<float, uint32_t> PickEntityInstance(
			const Ray& rayInViewSpace,
			const spieler::InstancesComponent& instancesComponent,
			const spieler::CollisionComponent* collisionComponent
		) const;

		uint32_t PickEntityInstanceTriangle(const Ray& ray, const spieler::MeshComponent& meshComponent) const;

	private:
		const spieler::Window& m_Window;

		std::span<const spieler::Entity*> m_PickableEntities;
		const spieler::Camera* m_ActiveCamera{ nullptr };
	};

} // namespace sandbox
