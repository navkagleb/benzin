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

#include <shaders/joint/mesh_types.hpp>

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

    const RayTracing_Tlas& RayTracing_Scene::GetActiveTlas() const
    {
        return m_Tlases[m_Device.GetActiveFrameIndex()];
    }

    void RayTracing_Scene::BuildBlases()
    {
        BenzinTraceScopeTime("RayTracing_Scene::BuildBlases");

        uint32_t drawPartCount = 0;
        for (const MeshRange& meshRange : m_Scene.m_MeshRanges)
        {
            drawPartCount += meshRange.m_DrawPartCount;
        }

        std::vector<DirectX::XMFLOAT3X4> localTransforms;
        localTransforms.reserve(drawPartCount);

        std::unique_ptr<Buffer> localTransformBuffer = m_Device.GetTemporalLinearAllocator().AllocateStructuredBuffer(
            "RayTracing_Scene::LocalTransforms",
            drawPartCount,
            sizeof(DirectX::XMFLOAT3X4));

        for (const MeshRange& meshRange : m_Scene.m_MeshRanges)
        {
            RayTracing_Blas& blas = m_Blases.emplace_back(meshRange.m_DrawPartCount);

            const auto meshDrawParts = ToSpan(m_Scene.m_MeshDrawParts.data() + meshRange.m_DrawPartOffset, meshRange.m_DrawPartCount);
            for (const MeshDrawPart& drawPart : meshDrawParts)
            {
                const benzin::MeshPart& part = m_Scene.m_MeshParts[drawPart.m_PartIndex];

                RayTracing_Blas::Geometry geometry{ .m_VertexBuffer = *m_Scene.m_VertexBuffer, .m_IndexBuffer = *m_Scene.m_IndexBuffer };
                geometry.m_VertexOffset = part.m_VertexOffset;
                geometry.m_VertexCount = part.m_VertexCount;
                geometry.m_IndexOffset = part.m_IndexOffset;
                geometry.m_IndexCount = part.m_IndexCount;
                geometry.m_TransformGpuAddress = localTransformBuffer->GetGpuVirtualAddress((uint32_t)localTransforms.size());

                blas.AddGeometry(geometry);

                const DirectX::XMMATRIX objectToLocal = DirectX::XMMatrixTranspose(drawPart.m_ObjectToLocal);
                localTransforms.push_back(*(DirectX::XMFLOAT3X4*)&objectToLocal);
            }
        }

        BufferWriter writer = MakeBufferWriter(*localTransformBuffer);
        writer.WriteArray(ToSpan(localTransforms));

        ComputeCmdList& cmdList = m_Device.GetGraphicsCmdQueue().GetCmdList();

        for (RayTracing_Blas& blas : m_Blases)
        {
            blas.AllocateBuffers(m_Device, "TODO");
            cmdList.AddResourceBarrier(TransitionBarrier{ *blas.GetScratchResource(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS });
        }

        cmdList.FlushResourceBarriers();

        for (RayTracing_Blas& blas : m_Blases)
        {
            cmdList.BuildRayTracingAccelerationStructure(blas);
            cmdList.AddResourceBarrier(UnorderedAccessBarrier{ *blas.GetBuffer() });
        }

        cmdList.FlushResourceBarriers();
    }

    void RayTracing_Scene::UpdateTlas()
    {
        BenzinProfile();

        RayTracing_Tlas& tlas = m_Tlases[m_Device.GetActiveFrameIndex()];
        tlas.ResetInstances((uint32_t)m_Scene.m_MeshDraws.size());

        for (const MeshDraw& draw : m_Scene.m_MeshDraws)
        {
            const DirectX::XMMATRIX scaling = DirectX::XMMatrixScaling(draw.m_Scale, draw.m_Scale, draw.m_Scale);
            const DirectX::XMMATRIX rotation = DirectX::XMMatrixRotationX(draw.m_Rotation.x) * DirectX::XMMatrixRotationY(draw.m_Rotation.y) * DirectX::XMMatrixRotationZ(draw.m_Rotation.z);
            const DirectX::XMMATRIX translation = DirectX::XMMatrixTranslation(draw.m_Translation.x, draw.m_Translation.y, draw.m_Translation.z);

            RayTracing_Tlas::Instance instance;
            instance.m_BlasGpuVirtualAddress = m_Blases[draw.m_MeshRangeIndex].GetGpuVirtualAddress();
            instance.m_LocalToWorld = scaling * rotation * translation;

            tlas.AddInstance(instance);
        }

        tlas.AllocateBuffers(m_Device, "RayTracing_Scene::Tlas");
    }

}
