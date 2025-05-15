#pragma once

#include <shaders/joint/texture_viewer_resources.hpp>

#include <benzin/graphics2/render_pass.hpp>

namespace benzin
{

    class TextureViewerTool;

    class TextureViewerPass : public RenderPass
    {
    public:
        TextureViewerPass(const TextureViewerTool& textureViewerTool);
        ~TextureViewerPass() override;

    private:
        bool IsDependentOnViewport() const override { return false; }

        void OnUpdate() override;
        void OnRender() const override;

    private:
        const TextureViewerTool& m_TextureViewerTool;

        TextureId m_ReferenceTextureId = g_InvalidTextureId;

        joint::TextureViewerConsts m_Consts{};
    };

}
