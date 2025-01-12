#include "benzin/config/bootstrap.hpp"
#include "benzin/engine/ray_tracing_scene.hpp"

#include <shaders/joint/mesh_types.hpp>

#include "benzin/core/asserter.hpp"
#include "benzin/core/command_line_args.hpp"
#include "benzin/engine/entity_components.hpp"
#include "benzin/engine/mesh.hpp"
#include "benzin/engine/scene.hpp"
#include "benzin/graphics/command_queue.hpp"
#include "benzin/graphics/device.hpp"
#include "benzin/graphics/rt_acceleration_structures.hpp"

namespace benzin
{

    using RayTracingMesh_LocalTransforms = std::vector<DirectX::XMFLOAT3X4>;
    using RayTracingMesh_Geometries = std::vector<RtGeometryVariant>;
    using RayTracingMesh_Blas = std::unique_ptr<BottomLevelAccelerationStructure>;

    //

    RayTracingScene::RayTracingScene(Device& device, Scene& scene)
        : m_Device{ device }
        , m_Scene{ scene }
    {
        m_Tlases.resize(CommandLineArgs::GetU32("FrameInFlightCount"));
    }

    RayTracingScene::~RayTracingScene()
    {
        BenzinLogTimeOnScopeExit("RayTracingScene::~RayTracingScene");

        const auto view = m_Scene.m_MeshRegistry.view<RayTracingMesh_Blas>();
        m_Scene.m_MeshRegistry.remove<RayTracingMesh_Blas>(view.begin(), view.end());
    }

    void RayTracingScene::BuildBlases()
    {
        BenzinLogTimeOnScopeExit("RayTracingScene::BuildBlases");

        ProcessMeshes();

        std::unique_ptr<benzin::Buffer> tempLocalTransformBuffer;
        UploadLocalTransformsToGpu(tempLocalTransformBuffer);
        CreateBlases(*tempLocalTransformBuffer);
    }

    uint64_t RayTracingScene::BuildTlas()
    {
        auto& tlas = m_Tlases[m_Device.GetActiveFrameIndex()];

        {
            std::vector<TopLevelInstance> topLevelInstances;

            const auto view = m_Scene.m_EntityRegistry.view<TransformComponent, MeshComponent>();
            for (const auto& [_, tc, mc] : view.each())
            {
                const auto& blas = m_Scene.m_MeshRegistry.get<RayTracingMesh_Blas>(mc.MeshHandle);

                topLevelInstances.push_back(TopLevelInstance
                {
                    .BottomLevelAccelerationStructure = *blas,
                    .HitGroupIndex = 0, // TODO: For now all instances have default hit group
                    .Transform = tc.GetLocalToWorldMatrix(),
                });
            }

            MakeUniquePtr(tlas, m_Device, TopLevelAccelerationStructureCreation
            {
                .DebugName = std::format("Tlas: {}", m_Scene.m_MeshRegistry.capacity()),
                .Instances = topLevelInstances,
            });
        }

        {
            auto& commandList = m_Device.GetGraphicsCommandQueue().GetCommandList();

            BenzinMakeScopedResourceBarriers(commandList, TransitionBarrier{ tlas->GetScratchResource(), ResourceState::UnorderedAccess });
            commandList.BuildRayTracingAccelerationStructure(*tlas);
        }

        return tlas->GetBuffer().GetGpuVirtualAddress();
    }

    void RayTracingScene::ProcessMeshes()
    {
        BenzinLogTimeOnScopeExit("RayTracingScene::ProcessMeshes");

        m_Scene.m_MeshRegistry.each([this](entt::entity meshHandle)
        {
            const auto& mesh = m_Scene.m_MeshRegistry.get<Mesh>(meshHandle);
            const auto& meshGpuStorage = m_Scene.m_MeshRegistry.get<MeshGpuStorage>(meshHandle);

            std::vector<RtGeometryVariant> geometries;
            geometries.reserve(mesh.SubMeshInstances.size());

            std::vector<DirectX::XMFLOAT3X4> localTransforms;
            localTransforms.reserve(mesh.SubMeshInstances.size());

            for (const joint::MeshInstance& instance : mesh.SubMeshInstances)
            {
                // TODO: There is duplication of Mesh due to using transform from MeshInstance

                const MeshData& subMesh = mesh.SubMeshes[instance.SubMeshIndex];
                const joint::MeshInfo meshInfo = mesh.SubMeshInfos[instance.SubMeshIndex];

                geometries.push_back(RtTriangledGeometry
                {
                    .VertexBuffer = *meshGpuStorage.VertexBuffer,
                    .IndexBuffer = *meshGpuStorage.IndexBuffer,
                    .VertexOffset = meshInfo.VertexOffset,
                    .IndexOffset = meshInfo.IndexOffset,
                    .VertexCount = (uint32_t)subMesh.Vertices.size(),
                    .IndexCount = (uint32_t)subMesh.Indices.size(),
                });

                const DirectX::XMMATRIX transposedMatrix = DirectX::XMMatrixTranspose(instance.Transform);
                localTransforms.push_back(*(DirectX::XMFLOAT3X4*)&transposedMatrix);
            }

            m_Scene.m_MeshRegistry.emplace<RayTracingMesh_Geometries>(meshHandle, geometries);
            m_Scene.m_MeshRegistry.emplace<RayTracingMesh_LocalTransforms>(meshHandle, localTransforms);
        });
    }

    void RayTracingScene::UploadLocalTransformsToGpu(std::unique_ptr<benzin::Buffer>& tempLocalTransformBuffer)
    {
        BenzinLogTimeOnScopeExit("RayTracingScene::UploadLocalTransformsToGpu");

        const auto view = m_Scene.m_MeshRegistry.view<RayTracingMesh_LocalTransforms>();

        uint32_t transformCount = 0;
        m_Scene.m_MeshRegistry.each([this, &transformCount](entt::entity meshHandle)
        {
            const auto& transforms = m_Scene.m_MeshRegistry.get<RayTracingMesh_LocalTransforms>(meshHandle);

            transformCount += (uint32_t)transforms.size();
        });

        MakeUniquePtr(tempLocalTransformBuffer, m_Device, BufferCreation
        {
            .DebugName = "RayTracingScene_TempLocalTransforms",
            .MemoryType = ResourceMemoryType::Upload,
            .Type = BufferType::Structured,
            .ElementSize = sizeof(DirectX::XMFLOAT3X4),
            .ElementCount = transformCount,
        });

        const MemoryWriter writer{ tempLocalTransformBuffer->GetCpuMappedData(), tempLocalTransformBuffer->GetSize() };
        uint32_t offsetElement = 0;

        m_Scene.m_MeshRegistry.each([this, &writer, &offsetElement](entt::entity meshHandle)
        {
            const auto& transforms = m_Scene.m_MeshRegistry.get<RayTracingMesh_LocalTransforms>(meshHandle);

            writer.WriteArray(std::span<const DirectX::XMFLOAT3X4>{ transforms }, offsetElement);
            offsetElement += (uint32_t)transforms.size();

            m_Scene.m_MeshRegistry.remove<RayTracingMesh_LocalTransforms>(meshHandle);
        });
    }

    void RayTracingScene::CreateBlases(const benzin::Buffer& tempLocalTransformBuffer)
    {
        BenzinLogTimeOnScopeExit("RayTracingScene::CreateBlases");

        auto& commandList = m_Device.GetGraphicsCommandQueue().GetCommandList();

        uint32_t offsetElement = 0;
        m_Scene.m_MeshRegistry.each([&](entt::entity meshHandle)
        {
            auto& geometries = m_Scene.m_MeshRegistry.get<RayTracingMesh_Geometries>(meshHandle);
            for (auto& geometry : geometries)
            {
                std::get<RtTriangledGeometry>(geometry).TransformGpuAddress = tempLocalTransformBuffer.GetGpuVirtualAddress(offsetElement++);
            }

            auto& blas = m_Scene.m_MeshRegistry.emplace<RayTracingMesh_Blas>(meshHandle);
            MakeUniquePtr(blas, m_Device, BottomLevelAccelerationStructureCreation
            {
                .DebugName = std::format("Blas: {}", geometries.size()),
                .Geometries = geometries,
            });

            BenzinMakeResourceBarriers(commandList, TransitionBarrier{ blas->GetScratchResource(), ResourceState::UnorderedAccess });
            commandList.BuildRayTracingAccelerationStructure(*blas);

            m_Scene.m_MeshRegistry.remove<RayTracingMesh_Geometries>(meshHandle);
        });

        // Wait for blases
        m_Scene.m_MeshRegistry.each([this, &commandList](entt::entity meshHandle)
        {
            auto& blas = m_Scene.m_MeshRegistry.get<RayTracingMesh_Blas>(meshHandle);
            BenzinMakeResourceBarriers(commandList, UnorderedAccessBarrier{ blas->GetBuffer() });
        });
    }

}
