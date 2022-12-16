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

	public:
		explicit PickingTechnique(spieler::Window& window);

	public:
		void SetActiveCamera(const spieler::Camera* activeCamera);
		void SetPickableEntities(const std::span<const spieler::Entity*>& pickableEntities);

	public:
		Result PickTriangle(float mouseX, float mouseY);

	private:
		spieler::Window& m_Window;

		std::span<const spieler::Entity*> m_PickableEntities;
		const spieler::Camera* m_ActiveCamera{ nullptr };
	};

} // namespace sandbox
