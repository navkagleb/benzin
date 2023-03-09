#pragma once

#include <benzin/system/window.hpp>
#include <benzin/engine/mesh.hpp>
#include <benzin/engine/camera.hpp>

namespace sandbox
{

	class PickingTechnique
	{
	public:
		struct Result
		{
			const benzin::Entity* PickedEntity{ nullptr };
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
		explicit PickingTechnique(const benzin::Window& window);

	public:
		void SetActiveCamera(const benzin::Camera* activeCamera);
		void SetPickableEntities(const std::span<const benzin::Entity*>& pickableEntities);

	public:
		Result PickTriangle(float mouseX, float mouseY) const;

	private:
		Ray GetRayInViewSpace(float mouseX, float mouseY) const;
		Ray GetRayInLocalSpace(const Ray& rayInViewSpace, const benzin::InstanceComponent& instanceComponent) const;

		std::pair<float, uint32_t> PickEntityInstance(
			const Ray& rayInViewSpace,
			const benzin::InstancesComponent& instancesComponent,
			const benzin::CollisionComponent* collisionComponent
		) const;

		uint32_t PickEntityInstanceTriangle(const Ray& ray, const benzin::MeshComponent& meshComponent) const;

	private:
		const benzin::Window& m_Window;

		std::span<const benzin::Entity*> m_PickableEntities;
		const benzin::Camera* m_ActiveCamera{ nullptr };
	};

} // namespace sandbox
