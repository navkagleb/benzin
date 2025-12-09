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

    RayTracingScene::RayTracingScene(const Scene& scene)
        : m_Scene{ scene }
    {}

    RayTracingScene::~RayTracingScene()
    {
        m_Blases.clear();
    }

    void RayTracingScene::BuildBlases(Device& device)
    {
        BenzinTraceScopeTime("RayTracingScene::BuildBlases");

        uint32_t drawCount = 0;
        for (const MeshGeometryDraw& geometryDraw : m_Scene.m_MeshGeometryDraws)
        {
            drawCount += geometryDraw.m_MeshDrawCount;
        }

        std::vector<DirectX::XMFLOAT3X4> localTransforms;
        localTransforms.reserve(drawCount);

        BufferCreation localTransformBufferCreation;
        localTransformBufferCreation.m_DebugName = "RayTracingScene::LocalTransforms";
        localTransformBufferCreation.m_HeapType = GpuHeapType::GpuUpload;
        localTransformBufferCreation.m_Type = BufferType::Structured;
        localTransformBufferCreation.m_ElementSizeInBytes = sizeof(DirectX::XMFLOAT3X4);
        localTransformBufferCreation.m_ElementCount = drawCount;

        auto localTransformBuffer = std::make_unique<Buffer>(device, localTransformBufferCreation);

        for (const MeshGeometryDraw& geometryDraw : m_Scene.m_MeshGeometryDraws)
        {
            RayTracing_Blas& blas = m_Blases.emplace_back(geometryDraw.m_MeshDrawCount);

            const auto draws = ToSpan(m_Scene.m_MeshDraws.data() + geometryDraw.m_MeshDrawOffset, geometryDraw.m_MeshDrawCount);
            for (const MeshDraw& draw : draws)
            {
                const Mesh& mesh = m_Scene.m_Geometry.m_Meshes[draw.m_MeshIndex];

                RayTracing_Blas::Geometry geometry{ .m_VertexBuffer = *m_Scene.m_VertexBuffer, .m_IndexBuffer = *m_Scene.m_IndexBuffer };
                geometry.m_VertexOffset = mesh.m_VertexOffset;
                geometry.m_VertexCount = mesh.m_VertexCount;
                geometry.m_IndexOffset = mesh.m_Lods[0].m_IndexOffset;
                geometry.m_IndexCount = mesh.m_Lods[0].m_IndexCount;
                geometry.m_TransformGpuAddress = localTransformBuffer->GetGpuVirtualAddress((uint32_t)localTransforms.size());

                blas.AddGeometry(geometry);

                const DirectX::XMMATRIX objectToLocal = DirectX::XMMatrixTranspose(draw.m_ObjectToLocal);
                localTransforms.push_back(*(DirectX::XMFLOAT3X4*)&objectToLocal);
            }
        }

        BufferWriter writer = MakeBufferWriter(*localTransformBuffer);
        writer.WriteArray(ToSpan(localTransforms));

        ComputeCmdList& cmdList = device.GetGraphicsCmdQueue().GetCmdList();

        for (RayTracing_Blas& blas : m_Blases)
        {
            blas.AllocateBuffers(device, "TODO");
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

    void RayTracingScene::UpdateTlasInstances(Device& device)
    {
        BenzinProfile();

        m_Tlas.ResetInstances((uint32_t)m_Scene.m_MeshGeometryDraws.size());

        for (uint32_t i = 0; i < m_Scene.m_MeshGeometryDraws.size(); ++i)
        {
            const MeshGeometryDraw& geometryDraw = m_Scene.m_MeshGeometryDraws[i];

            const DirectX::XMMATRIX scaling = DirectX::XMMatrixScaling(geometryDraw.m_Scale, geometryDraw.m_Scale, geometryDraw.m_Scale);
            const DirectX::XMMATRIX rotation = DirectX::XMMatrixRotationX(geometryDraw.m_Rotation.x) * DirectX::XMMatrixRotationY(geometryDraw.m_Rotation.y) * DirectX::XMMatrixRotationZ(geometryDraw.m_Rotation.z);
            const DirectX::XMMATRIX translation = DirectX::XMMatrixTranslation(geometryDraw.m_Translation.x, geometryDraw.m_Translation.y, geometryDraw.m_Translation.z);

            RayTracing_Tlas::Instance instance;
            instance.m_BlasGpuVirtualAddress = m_Blases[i].GetGpuVirtualAddress();
            instance.m_LocalToWorld = scaling * rotation * translation;

            m_Tlas.AddInstance(instance);
        }

        m_Tlas.AllocateInstanceBuffer(device, "RayTracing_Scene::Tlas");

        if (m_Tlas.GetBuffer() == nullptr || m_Tlas.GetScratchResource() == nullptr)
        {
            m_Tlas.AllocateBuffers(device, "RayTracing_Scene::Tlas");
        }
    }

}
