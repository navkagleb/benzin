#include "benzin/config/bootstrap.hpp"
#include "benzin/engine/ray_tracing_scene.hpp"

#include <shaders/joint/mesh_types.hpp>

#include "benzin/core/buffer_writer.hpp"
#include "benzin/core/command_line_args.hpp"
#include "benzin/core/profiler.hpp"
#include "benzin/engine/entity_components.hpp"
#include "benzin/engine/mesh.hpp"
#include "benzin/engine/scene.hpp"
#include "benzin/graphics/buffer.hpp"
#include "benzin/graphics/command_queue.hpp"
#include "benzin/graphics/device.hpp"
#include "benzin/graphics/ray_tracing_acceleration_structures.hpp"

namespace benzin
{

    RayTracing_Scene::RayTracing_Scene(Device& device, Scene& scene)
        : m_Device{ device }
        , m_Scene{ scene }
    {
        m_Tlases.resize(CommandLineArgs::GetU32("FrameInFlightCount"));
    }

    RayTracing_Scene::~RayTracing_Scene()
    {
        BenzinLogTimeOnScopeExit("RayTracing_Scene::~RayTracing_Scene");

        const auto view = m_Scene.m_MeshRegistry.view<RayTracing_Blas>();
        m_Scene.m_MeshRegistry.remove<RayTracing_Blas>(view.begin(), view.end());
    }

    const RayTracing_Tlas& RayTracing_Scene::GetActiveTlas() const
    {
        return m_Tlases[m_Device.GetActiveFrameIndex()];
    }

    void RayTracing_Scene::BuildBlases()
    {
        BenzinLogTimeOnScopeExit("RayTracing_Scene::BuildBlases");

        std::unique_ptr<benzin::Buffer> localTransformBuffer;
        ProcessMeshes(localTransformBuffer);
        CreateBlases();
    }

    void RayTracing_Scene::UpdateTlasBuffers()
    {
        BenzinProfile();

        const auto view = m_Scene.m_EntityRegistry.view<MeshComponent, Transform>();

        auto& tlas = m_Tlases[m_Device.GetActiveFrameIndex()];
        tlas.ResetInstances((uint32_t)view.size_hint());

        for (const auto& [_, mc, transform] : view.each())
        {
            if (!IsValidEnum(mc.MeshHandle))
            {
                continue;
            }

            const auto& blas = m_Scene.m_MeshRegistry.get<RayTracing_Blas>(mc.MeshHandle);

            tlas.AddInstance(RayTracing_Tlas::Instance
            {
                .Blas = blas,
                .HitGroupIndex = 0, // TODO: For now all instances have default hit group
                .Transform = transform.GetLocalToWorldMatrix(),
            });
        }

        tlas.AllocateBuffers(m_Device, "RayTracing_Scene_Tlas");
    }

    void RayTracing_Scene::ProcessMeshes(std::unique_ptr<Buffer>& localTransformBuffer)
    {
        BenzinLogTimeOnScopeExit("RayTracing_Scene::ProcessMeshes");

        const uint32_t transformCount = m_Scene.GetStats().MeshInstanceCount;

        std::vector<DirectX::XMFLOAT3X4> localTransforms;
        localTransforms.reserve(transformCount);

        MakeUniquePtr(localTransformBuffer, m_Device, BufferCreation
        {
            .DebugName = "RayTracingScene_TempLocalTransforms",
            .MemoryType = ResourceMemoryType::Upload,
            .Type = BufferType::Structured,
            .ElementSize = sizeof(DirectX::XMFLOAT3X4),
            .ElementCount = transformCount,
        });

        m_Scene.m_MeshRegistry.each([this, &localTransformBuffer, &localTransforms](entt::entity meshHandle)
        {
            const auto& mesh = m_Scene.m_MeshRegistry.get<Mesh>(meshHandle);
            const auto& meshGpuStorage = m_Scene.m_MeshRegistry.get<MeshGpuStorage>(meshHandle);

            auto& blas = m_Scene.m_MeshRegistry.emplace<RayTracing_Blas>(meshHandle, (uint32_t)mesh.SubMeshInstances.size());

            auto& blasStats = m_BlasStats.emplace_back();
            blasStats.DebugName = m_Scene.m_MeshRegistry.get<std::string>(meshHandle);
            blasStats.TriangleCountPerMesh.reserve(mesh.SubMeshInstances.size());

            for (const joint::MeshInstance& instance : mesh.SubMeshInstances)
            {
                // TODO: There is duplication of Mesh due to using transform from MeshInstance

                const MeshData& subMesh = mesh.SubMeshes[instance.SubMeshIndex];
                const joint::MeshInfo meshInfo = mesh.SubMeshInfos[instance.SubMeshIndex];

                blas.AddGeometry(RayTracing_Blas::Geometry
                {
                    .VertexBuffer = *meshGpuStorage.VertexBuffer,
                    .IndexBuffer = *meshGpuStorage.IndexBuffer,
                    .VertexOffset = meshInfo.VertexOffset,
                    .IndexOffset = meshInfo.IndexOffset,
                    .VertexCount = (uint32_t)subMesh.Vertices.size(),
                    .IndexCount = (uint32_t)subMesh.Indices.size(),
                    .TransformGpuAddress = localTransformBuffer->GetGpuVirtualAddress((uint32_t)localTransforms.size())
                });

                const DirectX::XMMATRIX transposedMatrix = DirectX::XMMatrixTranspose(instance.Transform);
                localTransforms.push_back(*(DirectX::XMFLOAT3X4*)&transposedMatrix);

                const auto triangleCount = (uint32_t)(subMesh.Indices.size() / 3);
                blasStats.TotalTriangleCount += triangleCount;
                blasStats.TriangleCountPerMesh.push_back(triangleCount);
            }
        });

        BufferWriter writer{ localTransformBuffer->GetCpuMappedData(), localTransformBuffer->GetSize() };
        writer.WriteData(std::as_bytes(std::span{ localTransforms }));
    }

    void RayTracing_Scene::CreateBlases()
    {
        BenzinLogTimeOnScopeExit("RayTracing_Scene::CreateBlases");

        // TODO: Remove dependece from command list

        auto& commandList = m_Device.GetGraphicsCommandQueue().GetCommandList();

        m_Scene.m_MeshRegistry.each([&](entt::entity meshHandle)
        {
            const std::string_view meshName = m_Scene.m_MeshRegistry.get<std::string>(meshHandle);

            auto& blas = m_Scene.m_MeshRegistry.get<RayTracing_Blas>(meshHandle);
            blas.AllocateBuffers(m_Device, meshName);

            BenzinMakeResourceBarriers(commandList, TransitionBarrier{ *blas.GetScratchResource(), ResourceState::UnorderedAccess });
            commandList.BuildRayTracingAccelerationStructure(blas);
        });

        // Wait for blases
        m_Scene.m_MeshRegistry.each([this, &commandList](entt::entity meshHandle)
        {
            auto& blas = m_Scene.m_MeshRegistry.get<RayTracing_Blas>(meshHandle);
            BenzinMakeResourceBarriers(commandList, UnorderedAccessBarrier{ *blas.GetBuffer() });
        });
    }

}
