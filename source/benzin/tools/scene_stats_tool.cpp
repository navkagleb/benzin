#include "benzin/config/bootstrap.hpp"
#include "benzin/tools/scene_stats_tool.hpp"

#include "benzin/engine/ray_tracing_scene.hpp"
#include "benzin/engine/scene.hpp"

namespace benzin
{

    SceneStatsTool::SceneStatsTool(const Scene& scene, const RayTracing_Scene& rayTracingScene)
        : ImGuiTool{ "SceneStatsTool" }
        , m_Scene{ scene }
        , m_RayTracingScene{ rayTracingScene }
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

            SpawnSceneStats();
            SpawnRayTracingnSceneStats();
        });
    }

    void SceneStatsTool::SpawnSceneStats() const
    {
        if (!SpawnImGuiCollapsingHeader("Scene"))
        {
            return;
        }

        const auto& sceneStats = m_Scene.GetStats();
        ImGui::Text(BenzinFormatData("MeshCount: {:L}", sceneStats.MeshCount));
        ImGui::Text(BenzinFormatData("MaterialCount: {:L}", sceneStats.MaterialCount));
        ImGui::Text(BenzinFormatData("MeshInstanceCount: {:L}", sceneStats.MeshInstanceCount));

        ImGui::Separator();
        ImGui::Text(BenzinFormatData("VertexCount: {:L}", sceneStats.VertexCount));
        ImGui::Text(BenzinFormatData("TriangleCount: {:L}", sceneStats.TriangleCount));
    }

    void SceneStatsTool::SpawnRayTracingnSceneStats() const
    {
        if (!SpawnImGuiCollapsingHeader("RayTracing_Scene"))
        {
            return;
        }

        const auto stats = m_RayTracingScene.GetBlasStats();

        ImGui::Text(BenzinFormatData("BlasCount: {}", stats.size()));

        for (const auto& blasStats : m_RayTracingScene.GetBlasStats())
        {
            const auto meshHeaderName = std::format("{}: {} meshes - {:L} triangles", blasStats.DebugName, blasStats.TriangleCountPerMesh.size(), blasStats.TotalTriangleCount);
            if (!ImGui::CollapsingHeader(meshHeaderName.c_str()))
            {
                continue;
            }

            for (const auto [i, triangleCount] : blasStats.TriangleCountPerMesh | std::views::enumerate)
            {
                ImGui::Text(BenzinFormatData("{}: {:L}", i, triangleCount));
            }
        }
    }

}
