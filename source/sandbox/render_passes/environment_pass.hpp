#pragma once

#include <benzin/graphics2/render_pass.hpp>

namespace benzin
{

    class Texture;

}

namespace sandbox
{

    class EnvironmentPass : public benzin::RenderPass
    {
    public:
        EnvironmentPass();
        ~EnvironmentPass() override;

        bool IsDependentOnViewport() const override { return true; }

        void OnZeroFrameInit() override;
        void OnUpdate() override;
        void OnRender() const override;

    private:
        std::unique_ptr<benzin::Texture> LoadEquirectangularTexture();
        void ComputeCubeMapTexture(benzin::Texture& equirectangularTexture);

    private:
        std::unique_ptr<benzin::Texture> m_CubeTexture;
    };

}
