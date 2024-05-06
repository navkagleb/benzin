#include "sandbox/bootstrap.hpp"
#include "sandbox/tools/scene_stats_tool.hpp"

#include <benzin/engine/scene.hpp>

namespace sandbox
{

    SceneStatsTool::SceneStatsTool(const benzin::Scene& scene)
        : ImGuiTool{ "SceneStatsTool", false }
        , m_Scene{ scene }
    {}

    void SceneStatsTool::OnImGuiRender()
    {
        RenderImGuiWindow([this]
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
            ImGui::Text(BenzinFormatCstr("VertexCount: {:L}", sceneStats.VertexCount));
            ImGui::Text(BenzinFormatCstr("TriangleCount: {:L}", sceneStats.TriangleCount));
            ImGui::Text(BenzinFormatCstr("PointLightCount: {:L}", sceneStats.PointLightCount));
        });
    }

}
