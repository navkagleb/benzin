#pragma once

#include "benzin/graphics/texture.hpp"
#include "benzin/graphics/graphics_command_list.hpp"
#include "benzin/engine/camera.hpp"
#include "benzin/engine/light.hpp"

namespace benzin
{

	class ShadowMap
	{
	public:
		ShadowMap() = default;
		ShadowMap(Device& device, uint32_t width, uint32_t height);

	public:
		const std::shared_ptr<TextureResource>& GetShadowMap() const { return m_ShadowMap; }
		const Viewport& GetViewport() const { return m_Viewport; }
		const ScissorRect& GetScissorRect() const { return m_ScissorRect; }

		void SetDirectionalLight(const Light* directionalLight) { m_DirectionalLight = directionalLight; }

		const Camera& GetCamera() const { return m_Camera; }

	public:
		void OnResize(Device& device, uint32_t width, uint32_t height);

		void SetSceneBounds(const DirectX::XMFLOAT3& sceneCenter, float sceneRadius);
		DirectX::XMMATRIX GetShadowTransform() const;

	private:
		void InitTextureResource(Device& device, uint32_t width, uint32_t height);
		void InitViewport(float width, float height);

	private:
		std::shared_ptr<TextureResource> m_ShadowMap;

		Viewport m_Viewport;
		ScissorRect m_ScissorRect;

		const Light* m_DirectionalLight{ nullptr };

		OrthographicProjection m_OrthographicProjection;
		Camera m_Camera{ &m_OrthographicProjection };
	};

} // namespace benzin
