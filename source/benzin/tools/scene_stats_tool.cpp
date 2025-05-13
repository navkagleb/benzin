#include "benzin/config/bootstrap.hpp"
#include "benzin/tools/scene_stats_tool.hpp"

#include <shaders/joint/mesh_types.hpp>

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
    }

    void SceneStatsTool::DrawSceneStats() const
    {
        if (!ImGui_MainCollapsingHeader("Scene", ImGuiTreeNodeFlags_DefaultOpen))
        {
            return;
        }

        uint32_t vertexCount = 0;
        uint32_t triangleCount = 0;
        uint32_t drawRangeCount = 0;
        uint32_t meshInstanceCount = 0;
        uint32_t meshletCount = 0;

        const auto view = m_Scene.m_MeshRegistry.view<MeshTag, Mesh>();
        for (const entt::entity meshHandle : view)
        {
            const auto& mesh = view.get<Mesh>(meshHandle);

            vertexCount += (uint32_t)mesh.Vertices.size();
            triangleCount += (uint32_t)mesh.Indices.size() / 3;
            drawRangeCount += (uint32_t)mesh.DrawRanges.size();
            meshInstanceCount += (uint32_t)mesh.Instances.size();
            meshletCount += (uint32_t)mesh.Meshlets.size();
        }

        ImGui::Text(BenzinFormatData("Vertices: {:L}", vertexCount));
        ImGui::Text(BenzinFormatData("Triangles: {:L}", triangleCount));
        ImGui::Text(BenzinFormatData("Draw ranges: {:L}", drawRangeCount));
        ImGui::Text(BenzinFormatData("Meshlets: {:L}", meshletCount));
        ImGui::Separator();

        ImGui::Text(BenzinFormatData("Materials: {:L}", m_Scene.m_UnifiedMaterials.size()));
        ImGui::Text(BenzinFormatData("Mesh instances: {:L}", meshInstanceCount));
        ImGui::Separator();

        for (const entt::entity meshHandle : view)
        {
            const auto& meshTag = view.get<MeshTag>(meshHandle);
            const auto& mesh = view.get<Mesh>(meshHandle);

            const auto meshHeaderName = std::format(
                "{}: {} draws - {:L} tris - {:L} meshlets",
                meshTag,
                mesh.DrawRanges.size(),
                mesh.Indices.size() / 3,
                mesh.Meshlets.size()
            );

            ImGui_CollapsingHeaderWithIndent(meshHeaderName, [&mesh]
            {
                for (const auto& [i, drawRange] : mesh.DrawRanges | std::views::enumerate)
                {
                    ImGui::Text(BenzinFormatData("{}: {:L} triangles, {:L} meshlets", i, drawRange.IndexRange.Count / 3, drawRange.MeshletRange.Count));
                }
            });
        }
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

            const auto view = m_Scene.m_MeshRegistry.view<RayTracing_Blas>();
            for (const entt::entity meshHandle : view)
            {
                const auto& blas = view.get<RayTracing_Blas>(meshHandle);

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

}
