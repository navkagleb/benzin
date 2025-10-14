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
        : ImGuiTool{ "Engine/SceneStatsTool" }
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
        if (!ImGui::MainCollapsingHeader("Scene", ImGuiTreeNodeFlags_DefaultOpen))
        {
            return;
        }

        uint32_t vertexCount = 0;
        uint32_t triangleCount = 0;
        uint32_t drawRangeCount = 0;
        uint32_t meshInstanceCount = 0;
        uint32_t meshletCount = 0;

        const auto view = m_Scene.m_MeshRegistry.view<MeshTag, Mesh, MeshGpuStorage>();
        for (const entt::entity meshHandle : view)
        {
            const auto& mesh = view.get<Mesh>(meshHandle);

            vertexCount += (uint32_t)mesh.Vertices.size();
            triangleCount += (uint32_t)mesh.Indices.size() / 3;
            drawRangeCount += (uint32_t)mesh.DrawRanges.size();
            meshInstanceCount += (uint32_t)mesh.Instances.size();
            meshletCount += (uint32_t)mesh.Meshlets.size();
        }

        ImGui::FmtText("Vertices: {:L}", vertexCount);
        ImGui::FmtText("Triangles: {:L}", triangleCount);
        ImGui::FmtText("Draw ranges: {:L}", drawRangeCount);
        ImGui::FmtText("Meshlets: {:L}", meshletCount);
        ImGui::Separator();

        ImGui::FmtText("Materials: {:L}", m_Scene.m_UnifiedMaterials.size());
        ImGui::FmtText("Mesh instances: {:L}", meshInstanceCount);
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

            ImGui::CollapsingHeaderWithIndent(meshHeaderName, [&]
            {
                const auto drawBufferSize = [](const char* name, const Buffer& buffer)
                {
                    ImGui::FmtBulletText("{}: {:.2f} mb ({:L} / {})", name, ToMb(buffer.GetAllocationSizeInBytes()), buffer.GetElementCount(), buffer.GetElementSizeInBytes());
                };

                const auto& meshGpuStorage = view.get<MeshGpuStorage>(meshHandle);
                drawBufferSize("Vertex buffer", *meshGpuStorage.VertexBuffer);
                drawBufferSize("Index buffer", *meshGpuStorage.IndexBuffer);
                drawBufferSize("Meshlet buffer", *meshGpuStorage.MeshletBuffer);
                drawBufferSize("Meshlet cull volume buffer", *meshGpuStorage.MeshletCullVolumeBuffer);
                drawBufferSize("Meshlet indirect vertex buffer", *meshGpuStorage.MeshletIndirectVertexBuffer);
                drawBufferSize("Meshlet index buffer", *meshGpuStorage.MeshletIndexBuffer);

                ImGui::Text("Draw ranges:");
                for (const auto& [i, drawRange] : mesh.DrawRanges | std::views::enumerate)
                {
                    ImGui::FmtText("{}: {:L} triangles, {:L} meshlets", i, drawRange.IndexRange.Count / 3, drawRange.MeshletRange.Count);
                }
            });
        }
    }

    void SceneStatsTool::DrawRayTracingAccelerationStructuresStats() const
    {
        if (!ImGui::MainCollapsingHeader("RayTracing_AccelerationStructures"))
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

            ImGui::FmtSeparatorText("BLASes ({:.2f} mb)", ToMb(totalSizeInBytes));
            ImGui::FmtBulletText("Buffer: {:.2f} mb", ToMb(buffersSizeInBytes));
            ImGui::FmtBulletText("ScratchResource: {:.2f} mb", ToMb(scratchResourcesSizeInBytes));
        }

        {
            const auto& tlas = m_RayTracingScene.GetActiveTlas();
            if (tlas.IsAllocated())
            {
                uint64_t totalSizeInBytes = 0;
                totalSizeInBytes += tlas.GetBuffer()->GetAllocationSizeInBytes();
                totalSizeInBytes += tlas.GetScratchResource()->GetAllocationSizeInBytes();
                totalSizeInBytes += tlas.GetInstanceBuffer()->GetAllocationSizeInBytes();

                ImGui::FmtSeparatorText("TLAS ({:.2f})", ToMb(totalSizeInBytes));
                ImGui::FmtBulletText("Buffer: {:.2f} mb", ToMb(tlas.GetBuffer()->GetAllocationSizeInBytes()));
                ImGui::FmtBulletText("ScratchResource: {:.2f} mb", ToMb(tlas.GetScratchResource()->GetAllocationSizeInBytes()));
                ImGui::FmtBulletText("InstanceBuffer: {:.2f} mb", ToMb(tlas.GetInstanceBuffer()->GetAllocationSizeInBytes()));
            }
        }
    }

}
