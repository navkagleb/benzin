#pragma once

#include "spieler/graphics/texture.hpp"
#include "spieler/graphics/graphics_command_list.hpp"
#include "spieler/engine/camera.hpp"

namespace spieler
{

    class RenderTargetCubeMap
    {
    public:
        RenderTargetCubeMap(spieler::Device& device, uint32_t width, uint32_t height);

    public:
        std::shared_ptr<spieler::TextureResource>& GetCubeMap() { return m_CubeMap; }
        std::shared_ptr<spieler::TextureResource>& GetDepthStencil() { return m_DepthStencil; }

        const DirectX::XMVECTOR& GetPosition() const { return m_Position; }
        void SetPosition(const DirectX::XMVECTOR& position);

        const spieler::Camera& GetCamera(uint32_t index) const { return m_Cameras[index]; }

        const spieler::Viewport& GetViewport() const { return m_Viewport; }
        const spieler::ScissorRect& GetScissorRect() const { return m_ScissorRect; }

    public:
        void OnResize(spieler::Device& device, uint32_t width, uint32_t height);

    private:
        void InitCubeMap(spieler::Device& device, uint32_t width, uint32_t height);
        void InitDepthStencil(spieler::Device& device, uint32_t width, uint32_t height);
        void InitCameras(uint32_t width, uint32_t height);
        void InitViewport(float width, float height);

    private:
        std::shared_ptr<spieler::TextureResource> m_CubeMap;
        std::shared_ptr<spieler::TextureResource> m_DepthStencil;

        DirectX::XMVECTOR m_Position{ 0.0f, 0.0f, 0.0f, 1.0f };

        spieler::PerspectiveProjection m_PerspectiveProjection;
        std::array<spieler::Camera, 6> m_Cameras;

        spieler::Viewport m_Viewport;
        spieler::ScissorRect m_ScissorRect;
    };

} // namespace spieler
