#pragma once

#include <spieler/system/window.hpp>
#include <spieler/engine/mesh.hpp>
#include <spieler/engine/camera.hpp>

#include "layers/instancing_and_culling_layer.hpp"

namespace sandbox
{

	class PickingTechnique
	{
	public:
		struct Result
		{
			const InstancingAndCullingLayer::RenderItem* PickedRenderItem{ nullptr };
			uint32_t InstanceIndex{ 0 };
			uint32_t TriangleIndex{ 0 };
		};

	public:
		explicit PickingTechnique(spieler::Window& window);

	public:
		void SetActiveCamera(const spieler::Camera* activeCamera);
		void SetPickableRenderItems(const std::span<InstancingAndCullingLayer::RenderItem*>& pickableRenderItems);

	public:
		Result PickTriangle(float mouseX, float mouseY);

	private:
		spieler::Window& m_Window;

		std::span<InstancingAndCullingLayer::RenderItem*> m_PickableRenderItems;
		const spieler::Camera* m_ActiveCamera{ nullptr };
	};

} // namespace sandbox
