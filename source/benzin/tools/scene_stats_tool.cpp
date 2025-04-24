#include "benzin/config/bootstrap.hpp"
#include "benzin/tools/scene_stats_tool.hpp"

#include "benzin/core/logger.hpp"
#include "benzin/engine/ray_tracing_scene.hpp"
#include "benzin/engine/scene.hpp"
#include "benzin/graphics/buffer.hpp"
#include "benzin/graphics/ray_tracing_acceleration_structures.hpp"

namespace benzin
{

    SceneStatsTool::SceneStatsTool(const Scene& scene, const RayTracing_Scene& rayTracingScene)
        : ImGuiTool{ "SceneStatsTool" }
        , m_Scene{ scene }
        , m_RayTracingScene{ rayTracingScene }
    {}

    void SceneStatsTool::DrawWindowContent()
    {
        std::locale::global(Logger::GetThoudandSeperatorApostrophe3());
        BenzinExecuteOnScopeExit([] { std::locale::global(std::locale::classic()); });

        DrawSceneStats();
        DrawRayTracingAccelerationStructuresStats();
        DrawRayTracingSceneStats();
    }

    void SceneStatsTool::DrawSceneStats() const
    {
        if (!ImGui_MainCollapsingHeader("Scene"))
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

    void SceneStatsTool::DrawRayTracingAccelerationStructuresStats() const
    {
        if (!ImGui_MainCollapsingHeader("RayTracing_AccelerationStructures"))
        {
            return;
        }

        {
            uint64_t buffersSizeInBytes = 0;
            uint64_t scratchResourcesSizeInBytes = 0;

            const auto view = m_Scene.GetMeshRegistry().view<RayTracing_Blas>();
            for (const auto& [_, blas] : view.each())
            {
                if (!blas.IsAllocated())
                {
                    continue;
                }

                buffersSizeInBytes += blas.GetBuffer()->GetAllocationSizeInBytes();
                scratchResourcesSizeInBytes += blas.GetScratchResource()->GetAllocationSizeInBytes();
            }

            const uint64_t totalSizeInBytes = buffersSizeInBytes + scratchResourcesSizeInBytes;

            ImGui::SeparatorText(BenzinFormatData("BLASes ({:.2f} mb)", ToMb(totalSizeInBytes)));
            ImGui::BulletText(BenzinFormatData("Buffer: {:.2f} mb", ToMb(buffersSizeInBytes)));
            ImGui::BulletText(BenzinFormatData("ScratchResource: {:.2f} mb", ToMb(scratchResourcesSizeInBytes)));
        }

        {
            const auto& tlas = m_RayTracingScene.GetActiveTlas();
            if (tlas.IsAllocated())
            {
                uint64_t totalSizeInBytes = 0;
                totalSizeInBytes += tlas.GetBuffer()->GetAllocationSizeInBytes();
                totalSizeInBytes += tlas.GetScratchResource()->GetAllocationSizeInBytes();
                totalSizeInBytes += tlas.GetInstanceBuffer()->GetAllocationSizeInBytes();

                ImGui::SeparatorText(BenzinFormatData("TLAS ({:.2f})", ToMb(totalSizeInBytes)));
                ImGui::BulletText(BenzinFormatData("Buffer: {:.2f} mb", ToMb(tlas.GetBuffer()->GetAllocationSizeInBytes())));
                ImGui::BulletText(BenzinFormatData("ScratchResource: {:.2f} mb", ToMb(tlas.GetScratchResource()->GetAllocationSizeInBytes())));
                ImGui::BulletText(BenzinFormatData("InstanceBuffer: {:.2f} mb", ToMb(tlas.GetInstanceBuffer()->GetAllocationSizeInBytes())));
            }
        }
    }

    void SceneStatsTool::DrawRayTracingSceneStats() const
    {
        if (!ImGui_MainCollapsingHeader("RayTracing_Scene"))
        {
            return;
        }

        const auto blasesStats = m_RayTracingScene.GetBlasesStats();

        ImGui::Text(BenzinFormatData("BlasCount: {}", blasesStats.size()));

        for (const auto& blasStats : blasesStats)
        {
            const auto meshHeaderName = std::format(
                "{}: {} meshes - {:L} triangles",
                blasStats.DebugName,
                blasStats.TriangleCountPerMesh.size(),
                blasStats.TotalTriangleCount
            );

            ImGui_CollapsingHeaderWithIndent(meshHeaderName, [&blasStats]
            {
                for (const auto [i, triangleCount] : blasStats.TriangleCountPerMesh | std::views::enumerate)
                {
                    ImGui::Text(BenzinFormatData("{}: {:L}", i, triangleCount));
                }
            });
        }
    }

}
