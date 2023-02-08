#include "benzin/config/bootstrap.hpp"

#include "benzin/graphics/shadow_map.hpp"

#include "benzin/graphics/device.hpp"
#include "benzin/graphics/resource_view_builder.hpp"

namespace benzin
{

	ShadowMap::ShadowMap(Device& device, uint32_t width, uint32_t height)
	{
		OnResize(device, width, height);
	}

	void ShadowMap::OnResize(Device& device, uint32_t width, uint32_t height)
	{
		InitTextureResource(device, width, height);
		InitViewport(static_cast<float>(width), static_cast<float>(height));
	}

	void ShadowMap::SetSceneBounds(const DirectX::XMFLOAT3& sceneCenter, float sceneRadius)
	{
		const DirectX::XMVECTOR targetPosition = DirectX::XMLoadFloat3(&sceneCenter);

		// Light view matrix
		{
			const DirectX::XMVECTOR lightDirection = DirectX::XMLoadFloat3(&m_DirectionalLight->Direction);

			const DirectX::XMVECTOR lightPosition = DirectX::XMVectorScale(lightDirection, -2.0f * sceneRadius);
			const DirectX::XMVECTOR lightFrontDirection = DirectX::XMVectorSubtract(targetPosition, lightPosition);

			m_Camera.SetPosition(lightPosition);
			m_Camera.SetFrontDirection(lightFrontDirection);
		}

		// Light Projection matrix
		{
			DirectX::XMFLOAT3 sphereCenterInLightSpace;
			DirectX::XMStoreFloat3(&sphereCenterInLightSpace, DirectX::XMVector3TransformCoord(targetPosition, m_Camera.GetViewMatrix()));

			m_OrthographicProjection.SetViewRect(OrthographicProjection::ViewRect
			{
				.LeftPlane{ sphereCenterInLightSpace.x - sceneRadius },
				.RightPlane{ sphereCenterInLightSpace.x + sceneRadius },
				.BottomPlane{ sphereCenterInLightSpace.y - sceneRadius },
				.TopPlane{ sphereCenterInLightSpace.y + sceneRadius },
				.NearPlane{ sphereCenterInLightSpace.z - sceneRadius },
				.FarPlane{ sphereCenterInLightSpace.z + sceneRadius }
			});
		}
	}

	DirectX::XMMATRIX ShadowMap::GetShadowTransform() const
	{
		// The transformation matrix transforms from world space to the shadow map texture space

		static const DirectX::XMMATRIX fromNDCSpaceToTextureSpaceTransformMatrix
		{
			0.5f, 0.0f, 0.0f, 0.0f,
			0.0f, -0.5f, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f,
			0.5f, 0.5f, 0.0f, 1.0f
		};

		return m_Camera.GetViewProjectionMatrix() * fromNDCSpaceToTextureSpaceTransformMatrix;
	}

	void ShadowMap::InitTextureResource(Device& device, uint32_t width, uint32_t height)
	{
		const TextureResource::Config textureConfig
		{
			.Type{ TextureResource::Type::Texture2D },
			.Width{ width },
			.Height{ height },
			.Format{ GraphicsFormat::D24UnsignedNormS8UnsignedInt },
			.Flags{ TextureResource::Flags::BindAsDepthStencil }
		};

		const TextureShaderResourceViewConfig srvConfig
		{
			.Format{ GraphicsFormat::R24UnsignedNormX8Typeless }
		};

		m_ShadowMap = device.CreateTextureResource(textureConfig, "ShadowMap");
		m_ShadowMap->PushShaderResourceView(device.GetResourceViewBuilder().CreateShaderResourceView(*m_ShadowMap, srvConfig));
		m_ShadowMap->PushDepthStencilView(device.GetResourceViewBuilder().CreateDepthStencilView(*m_ShadowMap));
	}

	void ShadowMap::InitViewport(float width, float height)
	{
		m_Viewport.Width = width;
		m_Viewport.Height = height;

		m_ScissorRect.Width = width;
		m_ScissorRect.Height = height;
	}

} // namespace benzin
