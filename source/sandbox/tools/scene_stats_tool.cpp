#include "sandbox/bootstrap.hpp"
#include "sandbox/tools/scene_stats_tool.hpp"

#include <benzin/engine/scene.hpp>

namespace sandbox
{

    SceneStatsTool::SceneStatsTool(const benzin::Scene& scene)
        : ImGuiTool{ "SceneStatsTool" }
        , m_Scene{ scene }
    {}

    void SceneStatsTool::SpawnImGui()
    {
        SpawnImGuiWindow([this]
        {
            struct ThoudandSeperatorApostrophe3 : std::numpunct<char>
            {
                char do_thousands_sep() const override { return '\''; }

                std::string do_grouping() const override { return "\3"; }
            };

            static const std::locale customLocale{ std::locale::classic(), new ThoudandSeperatorApostrophe3 };

            std::locale::global(customLocale);
            BenzinExecuteOnScopeExit([] { std::locale::global(std::locale::classic()); });

            const auto& sceneStats = m_Scene.GetStats();
            ImGui::Text(BenzinFormatData("MeshCount: {:L}", sceneStats.MeshCount));
            ImGui::Text(BenzinFormatData("MaterialCount: {:L}", sceneStats.MaterialCount));
            ImGui::Text(BenzinFormatData("MeshInstanceCount: {:L}", sceneStats.MeshInstanceCount));

            ImGui::Separator();
            ImGui::Text(BenzinFormatData("VertexCount: {:L}", sceneStats.VertexCount));
            ImGui::Text(BenzinFormatData("TriangleCount: {:L}", sceneStats.TriangleCount));
        });
    }

}
