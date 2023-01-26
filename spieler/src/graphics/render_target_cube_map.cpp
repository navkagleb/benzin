#include "spieler/config/bootstrap.hpp"

#include "spieler/graphics/render_target_cube_map.hpp"

#include "spieler/graphics/device.hpp"
#include "spieler/graphics/resource_view_builder.hpp"

namespace spieler
{

    RenderTargetCubeMap::RenderTargetCubeMap(spieler::Device& device, uint32_t width, uint32_t height)
        : m_PerspectiveProjection{ DirectX::XMConvertToRadians(90.0f), 1.0f, 0.1f, 1000.0f }
    {
        OnResize(device, width, height);
    }

    void RenderTargetCubeMap::SetPosition(const DirectX::XMVECTOR& position)
    {
        m_Position = position;

        for (auto& camera : m_Cameras)
        {
            camera.SetPosition(position);
        }
    }

    void RenderTargetCubeMap::OnResize(spieler::Device& device, uint32_t width, uint32_t height)
    {
        InitCubeMap(device, width, height);
        InitDepthStencil(device, width, height);
        InitCameras(width, height);
        InitViewport(static_cast<float>(width), static_cast<float>(height));
    }

    void RenderTargetCubeMap::InitCubeMap(spieler::Device& device, uint32_t width, uint32_t height)
    {
        const spieler::TextureResource::Config config
        {
            .Type{ spieler::TextureResource::Type::Texture2D },
            .Width{ width },
            .Height{ height },
            .ArraySize{ 6 },
            .IsCubeMap{ true },
            .Format{ spieler::GraphicsFormat::R8G8B8A8UnsignedNorm },
            .Flags{ spieler::TextureResource::Flags::BindAsRenderTarget }
        };

        const spieler::TextureResource::ClearColor clearColor
        {
            .Color{ 0.1f, 0.1f, 0.1f, 1.0f }
        };

        m_CubeMap = device.CreateTextureResource(config, clearColor);
        m_CubeMap->PushShaderResourceView(device.GetResourceViewBuilder().CreateShaderResourceView(*m_CubeMap));

        for (uint32_t i = 0; i < 6; ++i)
        {
            const spieler::TextureRenderTargetViewConfig rtvConfig
            {
                .FirstArrayIndex{ i },
                .ArraySize{ 1 }
            };

            m_CubeMap->PushRenderTargetView(device.GetResourceViewBuilder().CreateRenderTargetView(*m_CubeMap, rtvConfig));
        }
    }

    void RenderTargetCubeMap::InitDepthStencil(spieler::Device& device, uint32_t width, uint32_t height)
    {
        const spieler::TextureResource::Config config
        {
            .Type{ spieler::TextureResource::Type::Texture2D },
            .Width{ width },
            .Height{ height },
            .ArraySize{ 1 },
            .Format{ spieler::GraphicsFormat::D24UnsignedNormS8UnsignedInt },
            .Flags{ spieler::TextureResource::Flags::BindAsDepthStencil }
        };

        const spieler::TextureResource::ClearDepthStencil clearDepthStencil
        {
            .Depth{ 1.0f },
            .Stencil{ 0 }
        };

        m_DepthStencil = device.CreateTextureResource(config, clearDepthStencil);
        m_DepthStencil->PushDepthStencilView(device.GetResourceViewBuilder().CreateDepthStencilView(*m_DepthStencil));
    }

    void RenderTargetCubeMap::InitCameras(uint32_t width, uint32_t height)
    {
        const std::array<DirectX::XMVECTOR, 6> frontDirections
        {
            DirectX::XMVECTOR{ 1.0f, 0.0f, 0.0f, 0.0f },    // +X
            DirectX::XMVECTOR{ -1.0f, 0.0f, 0.0f, 0.0f },   // -X
            DirectX::XMVECTOR{ 0.0f, 1.0f, 0.0f, 0.0f },    // +Y
            DirectX::XMVECTOR{ 0.0f, -1.0f, 0.0f, 0.0f },   // -Y
            DirectX::XMVECTOR{ 0.0f, 0.0f, 1.0f, 0.0f },    // +Z
            DirectX::XMVECTOR{ 0.0f, 0.0f, -1.0f, 0.0f }    // -Z
        };

        const std::array<DirectX::XMVECTOR, 6> upDirections
        {
            DirectX::XMVECTOR{ 0.0f, 1.0f, 0.0f, 0.0f },    // +X
            DirectX::XMVECTOR{ 0.0f, 1.0f, 0.0f, 0.0f },    // -X
            DirectX::XMVECTOR{ 0.0f, 0.0f, -1.0f, 0.0f },   // +Y
            DirectX::XMVECTOR{ 0.0f, 0.0f, 1.0f, 0.0f },    // -Y
            DirectX::XMVECTOR{ 0.0f, 1.0f, 0.0f, 0.0f },    // +Z
            DirectX::XMVECTOR{ 0.0f, 1.0f, 0.0f, 0.0f }     // -Z
        };

        m_PerspectiveProjection.SetAspectRatio(1.0f);

        for (size_t i = 0; i < m_Cameras.size(); ++i)
        {
            auto& camera = m_Cameras[i];
            camera.SetPosition(m_Position);
            camera.SetFrontDirection(frontDirections[i]);
            camera.SetUpDirection(upDirections[i]);
            camera.SetProjection(&m_PerspectiveProjection);
        }
    }

    void RenderTargetCubeMap::InitViewport(float width, float height)
    {
        m_Viewport.Width = width;
        m_Viewport.Height = height;

        m_ScissorRect.Width = width;
        m_ScissorRect.Height = height;
    }

} // namespace spieler
