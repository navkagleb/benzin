#pragma once

#include "benzin/graphics2/imgui_pass.hpp"

namespace benzin
{

    class Scene;

    class SceneTool : public ImGuiTool
    {
    public:
        explicit SceneTool(Scene& scene);

    private:
        void SpawnImGui() override;

        void SpawnSun();
        void SpawnSphericalLights();

    private:
        Scene& m_Scene;
    };

}
