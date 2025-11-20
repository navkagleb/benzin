#include <benzin/config/bootstrap.hpp>
#include <benzin/engine/ray_tracing_scene.hpp>

#include <benzin/core/buffer_writer.hpp>
#include <benzin/core/profiler.hpp>
#include <benzin/engine/mesh.hpp>
#include <benzin/engine/scene.hpp>
#include <benzin/graphics/buffer.hpp>
#include <benzin/graphics/cmd_queue.hpp>
#include <benzin/graphics/device.hpp>
#include <benzin/graphics/gpu_heap.hpp>

namespace benzin
{

    RayTracing_Scene::RayTracing_Scene(Device& device, Scene& scene)
        : m_Device{ device }
        , m_Scene{ scene }
    {}

    RayTracing_Scene::~RayTracing_Scene()
    {
        m_Blases.clear();
    }

    void RayTracing_Scene::BuildBlases()
    {
        BenzinTraceScopeTime("RayTracing_Scene::BuildBlases");

        uint32_t drawCount = 0;
        for (const Scene::MeshRange& meshRange : m_Scene.m_MeshRanges)
        {
            drawCount += meshRange.m_MeshDrawCount;
        }

        std::vector<DirectX::XMFLOAT3X4> localTransforms;
        localTransforms.reserve(drawCount);

        auto localTransformBuffer = std::make_unique<Buffer>(m_Device, BufferCreation
        {
            .m_DebugName = "RayTracing_Scene::LocalTransforms",
            .m_HeapType = GpuHeapType::GpuUpload,
            .m_Type = BufferType::Structured,
            .m_ElementSizeInBytes = sizeof(DirectX::XMFLOAT3X4),
            .m_ElementCount = drawCount,
        });

        for (const Scene::MeshRange& meshRange : m_Scene.m_MeshRanges)
        {
            RayTracing_Blas& blas = m_Blases.emplace_back(meshRange.m_MeshDrawCount);

            const auto draws = ToSpan(m_Scene.m_MeshDraws.data() + meshRange.m_MeshDrawOffset, meshRange.m_MeshDrawCount);
            for (const MeshDraw& draw : draws)
            {
                const Mesh& mesh = m_Scene.m_Meshes[draw.m_MeshIndex];

                RayTracing_Blas::Geometry geometry{ .m_VertexBuffer = *m_Scene.m_VertexBuffer, .m_IndexBuffer = *m_Scene.m_IndexBuffer };
                geometry.m_VertexOffset = mesh.m_VertexOffset;
                geometry.m_VertexCount = mesh.m_VertexCount;
                geometry.m_IndexOffset = mesh.m_IndexOffset;
                geometry.m_IndexCount = mesh.m_IndexCount;
                geometry.m_TransformGpuAddress = localTransformBuffer->GetGpuVirtualAddress((uint32_t)localTransforms.size());

                blas.AddGeometry(geometry);

                const DirectX::XMMATRIX objectToLocal = DirectX::XMMatrixTranspose(draw.m_ObjectToLocal);
                localTransforms.push_back(*(DirectX::XMFLOAT3X4*)&objectToLocal);
            }
        }

        BufferWriter writer = MakeBufferWriter(*localTransformBuffer);
        writer.WriteArray(ToSpan(localTransforms));

        ComputeCmdList& cmdList = m_Device.GetGraphicsCmdQueue().GetCmdList();

        for (RayTracing_Blas& blas : m_Blases)
        {
            blas.AllocateBuffers(m_Device, "TODO");
            cmdList.AddTransition(*blas.GetScratchResource(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        }

        cmdList.FlushBarriers();

        for (RayTracing_Blas& blas : m_Blases)
        {
            cmdList.BuildRayTracingAccelerationStructure(blas);
            cmdList.AddUnorderedAccess(*blas.GetBuffer());
        }

        cmdList.FlushBarriers();
    }

    void RayTracing_Scene::UpdateTlasInstances()
    {
        BenzinProfile();

        m_Tlas.ResetInstances((uint32_t)m_Scene.m_MeshRangeDraws.size());

        for (const MeshRangeDraw& rangeDraw : m_Scene.m_MeshRangeDraws)
        {
            const DirectX::XMMATRIX scaling = DirectX::XMMatrixScaling(rangeDraw.m_Scale, rangeDraw.m_Scale, rangeDraw.m_Scale);
            const DirectX::XMMATRIX rotation = DirectX::XMMatrixRotationX(rangeDraw.m_Rotation.x) * DirectX::XMMatrixRotationY(rangeDraw.m_Rotation.y) * DirectX::XMMatrixRotationZ(rangeDraw.m_Rotation.z);
            const DirectX::XMMATRIX translation = DirectX::XMMatrixTranslation(rangeDraw.m_Translation.x, rangeDraw.m_Translation.y, rangeDraw.m_Translation.z);

            RayTracing_Tlas::Instance instance;
            instance.m_BlasGpuVirtualAddress = m_Blases[rangeDraw.m_MeshRangeIndex].GetGpuVirtualAddress();
            instance.m_LocalToWorld = scaling * rotation * translation;

            m_Tlas.AddInstance(instance);
        }

        m_Tlas.AllocateInstanceBuffer(m_Device, "RayTracing_Scene::Tlas");

        if (m_Tlas.GetBuffer() == nullptr || m_Tlas.GetScratchResource() == nullptr)
        {
            m_Tlas.AllocateBuffers(m_Device, "RayTracing_Scene::Tlas");
        }
    }

}
