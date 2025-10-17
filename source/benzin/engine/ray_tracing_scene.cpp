#include "benzin/config/bootstrap.hpp"
#include "benzin/engine/ray_tracing_scene.hpp"

#include <shaders/joint/mesh_types.hpp>

#include "benzin/core/buffer_writer.hpp"
#include "benzin/core/cmd_line_args.hpp"
#include "benzin/core/profiler.hpp"
#include "benzin/engine/entity_components.hpp"
#include "benzin/engine/mesh.hpp"
#include "benzin/engine/scene.hpp"
#include "benzin/graphics/buffer.hpp"
#include "benzin/graphics/cmd_queue.hpp"
#include "benzin/graphics/device.hpp"
#include "benzin/graphics/gpu_heap.hpp"

namespace benzin
{

    RayTracing_Scene::RayTracing_Scene(Device& device, Scene& scene)
        : m_Device{ device }
        , m_Scene{ scene }
    {}

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

        std::unique_ptr<benzin::Buffer> localTransformBuffer; // Must live until CreateBlases works
        ProcessMeshes(localTransformBuffer);
        CreateBlases();
    }

    void RayTracing_Scene::UpdateTlasBuffers()
    {
        BenzinProfile();

        const auto view = m_Scene.m_EntityRegistry.view<MeshComponent, Transform>();
        const auto blasView = m_Scene.m_MeshRegistry.view<RayTracing_Blas>();

        auto& tlas = m_Tlases[m_Device.GetActiveFrameIndex()];
        tlas.ResetInstances((uint32_t)view.size_hint());

        for (const entt::entity entityHandle : view)
        {
            const auto& meshComponent = view.get<MeshComponent>(entityHandle);
            const auto& transform = view.get<Transform>(entityHandle);
            const auto& blas = blasView.get<RayTracing_Blas>(meshComponent.GetMeshHandle());

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

        const auto view = m_Scene.m_MeshRegistry.view<Mesh, MeshGpuStorage>();

        uint32_t meshInstanceCount = 0;
        for (const entt::entity meshHandle : view)
        {
            const auto& mesh = view.get<Mesh>(meshHandle);
            meshInstanceCount += (uint32_t)mesh.m_Instances.size();
        }

        std::vector<DirectX::XMFLOAT3X4> localTransforms;
        localTransforms.reserve(meshInstanceCount);

        localTransformBuffer = m_Device.GetTemporalLinearAllocator().AllocateStructuredBuffer(
            "RayTracingScene_LocalTransforms",
            meshInstanceCount,
            sizeof(DirectX::XMFLOAT3X4)
        );

        for (const entt::entity meshHandle : view)
        {
            const auto& mesh = view.get<Mesh>(meshHandle);
            const auto& meshGpuStorage = view.get<MeshGpuStorage>(meshHandle);

            const auto instanceCount = (uint32_t)mesh.m_Instances.size();

            auto& blas = m_Scene.m_MeshRegistry.emplace<RayTracing_Blas>(meshHandle, instanceCount);

            for (const MeshInstance& instance : mesh.m_Instances)
            {
                // TODO: There is duplication of Mesh due to using transform from MeshInstance
                // TODO: Can InstanceTransformBuffer be used here!

                const MeshDrawRange& drawRange = mesh.m_DrawRanges[instance.m_DrawRangeIndex];

                blas.AddGeometry(RayTracing_Blas::Geometry
                {
                    .VertexBuffer = *meshGpuStorage.VertexBuffer,
                    .IndexBuffer = *meshGpuStorage.IndexBuffer,
                    .VertexRange = drawRange.m_VertexRange,
                    .IndexRange = drawRange.m_IndexRange,
                    .TransformGpuAddress = localTransformBuffer->GetGpuVirtualAddress((uint32_t)localTransforms.size())
                });

                const DirectX::XMMATRIX objectToLocalMatrix = DirectX::XMMatrixTranspose(instance.m_ObjectToLocalMatrix);
                localTransforms.push_back(*(DirectX::XMFLOAT3X4*)&objectToLocalMatrix);
            }
        }

        BufferWriter writer = MakeBufferWriter(*localTransformBuffer);
        writer.WriteArray(ToSpan(localTransforms));
    }

    void RayTracing_Scene::CreateBlases()
    {
        BenzinLogTimeOnScopeExit("RayTracing_Scene::CreateBlases");

        auto& cmdList = m_Device.GetGraphicsCmdQueue().GetCmdList();

        const auto view = m_Scene.m_MeshRegistry.view<MeshTag, RayTracing_Blas>();

        for (const entt::entity meshHandle : view)
        {
            const auto& meshTag = view.get<MeshTag>(meshHandle);

            auto& blas = view.get<RayTracing_Blas>(meshHandle);
            blas.AllocateBuffers(m_Device, meshTag);

            cmdList.AddResourceBarrier(TransitionBarrier{ *blas.GetScratchResource(), ResourceState::UnorderedAccess });
        }

        cmdList.FlushResourceBarriers();

        // Wait for blases
        for (const entt::entity meshHandle : view)
        {
            auto& blas = view.get<RayTracing_Blas>(meshHandle);

            cmdList.BuildRayTracingAccelerationStructure(blas);
            cmdList.AddResourceBarrier(UnorderedAccessBarrier{ *blas.GetBuffer() });
        }

        cmdList.FlushResourceBarriers();
    }

}
