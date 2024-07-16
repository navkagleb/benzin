#pragma once

#include <benzin/engine/imgui_pass.hpp>

namespace benzin
{

    class Scene;

}

namespace sandbox
{

    class SceneStatsTool : public benzin::ImGuiTool
    {
    public:
        explicit SceneStatsTool(const benzin::Scene& scene);

    private:
        void SpawnImGui() override;

    private:
        const benzin::Scene& m_Scene;
    };

}
