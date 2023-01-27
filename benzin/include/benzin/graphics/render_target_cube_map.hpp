#pragma once

#include "benzin/graphics/texture.hpp"
#include "benzin/graphics/graphics_command_list.hpp"
#include "benzin/engine/camera.hpp"

namespace benzin
{

    class RenderTargetCubeMap
    {
    public:
        RenderTargetCubeMap(Device& device, uint32_t width, uint32_t height);

    public:
        std::shared_ptr<TextureResource>& GetCubeMap() { return m_CubeMap; }
        std::shared_ptr<TextureResource>& GetDepthStencil() { return m_DepthStencil; }

        const DirectX::XMVECTOR& GetPosition() const { return m_Position; }
        void SetPosition(const DirectX::XMVECTOR& position);

        const Camera& GetCamera(uint32_t index) const { return m_Cameras[index]; }

        const Viewport& GetViewport() const { return m_Viewport; }
        const ScissorRect& GetScissorRect() const { return m_ScissorRect; }

    public:
        void OnResize(Device& device, uint32_t width, uint32_t height);

    private:
        void InitCubeMap(Device& device, uint32_t width, uint32_t height);
        void InitDepthStencil(Device& device, uint32_t width, uint32_t height);
        void InitCameras(uint32_t width, uint32_t height);
        void InitViewport(float width, float height);

    private:
        std::shared_ptr<TextureResource> m_CubeMap;
        std::shared_ptr<TextureResource> m_DepthStencil;

        DirectX::XMVECTOR m_Position{ 0.0f, 0.0f, 0.0f, 1.0f };

        PerspectiveProjection m_PerspectiveProjection;
        std::array<Camera, 6> m_Cameras;

        Viewport m_Viewport;
        ScissorRect m_ScissorRect;
    };

} // namespace benzin
