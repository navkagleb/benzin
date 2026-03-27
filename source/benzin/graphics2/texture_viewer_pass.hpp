#pragma once

#include <shaders/joint/texture_viewer_resources.hpp>

#include <benzin/graphics2/render_pass.hpp>

namespace benzin
{

    struct TextureViewerData;

    class TextureViewerPass : public RenderPass
    {
    public:
        explicit TextureViewerPass(const TextureViewerData& viewerData);
        ~TextureViewerPass() override;

    private:
        bool IsDependentOnViewport() const override { return false; }

        void OnUpdate() override;
        void OnRender() const override;

    private:
        const TextureViewerData& m_ViewerData;

        TextureId m_ReferenceTextureId = g_MaxEnum<TextureId>;
        joint::TextureViewerConsts m_Consts = {};
    };

}
