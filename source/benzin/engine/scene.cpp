#include "benzin/config/bootstrap.hpp"
#include "benzin/engine/scene.hpp"

#include <shaders/joint/structured_buffer_types.hpp>
#include <shaders/joint/constant_buffer_types.hpp>

#include "benzin/engine/entity_components.hpp"
#include "benzin/graphics/buffer.hpp"
#include "benzin/graphics/command_queue.hpp"
#include "benzin/graphics/device.hpp"
#include "benzin/graphics/mapped_data.hpp"
#include "benzin/graphics/rt_acceleration_structures.hpp"
#include "benzin/graphics/texture.hpp"

namespace benzin
{

    namespace
    {

        constexpr uint32_t g_MaxPointLightCount = 200;

        MeshCollectionGPUStorage CreateMeshCollectionGPUStorage(Device& device, std::string_view debugName, const MeshCollection& meshCollection)
        {
            size_t totalVertexCount = 0;
            size_t totalIndexCount = 0;
            for (const auto& mesh : meshCollection.Meshes)
            {
                totalVertexCount += mesh.Vertices.size();
                totalIndexCount += mesh.Indices.size();
            }

            auto vertexBuffer = std::make_unique<Buffer>(device, BufferCreation
            {
                .DebugName = std::format("{}_VertexBuffer", debugName),
                .ElementSize = sizeof(joint::MeshVertex),
                .ElementCount = (uint32_t)totalVertexCount,
                .Flags = BufferFlag::StructuredBuffer,
                .IsNeedStructuredBufferView = true,
            });

            auto indexBuffer = std::make_unique<Buffer>(device, BufferCreation
            {
                .DebugName = std::format("{}_IndexBuffer", debugName),
                .Format = GraphicsFormat::R32Uint,
                .ElementSize = sizeof(uint32_t),
                .ElementCount = (uint32_t)totalIndexCount,
                .IsNeedFormatBufferView = true,
            });

            auto meshInfoBuffer = std::make_unique<Buffer>(device, BufferCreation
            {
                .DebugName = std::format("{}_MeshInfoBuffer", debugName),
                .ElementSize = sizeof(joint::MeshInfo),
                .ElementCount = (uint32_t)meshCollection.Meshes.size(),
                .Flags = BufferFlag::StructuredBuffer,
                .IsNeedStructuredBufferView = true,
            });

            std::unique_ptr<Buffer> meshParentTransformBuffer;
            if (!meshCollection.MeshParentTransforms.empty())
            {
                meshParentTransformBuffer = std::make_unique<Buffer>(device, BufferCreation
                {
                    .DebugName = std::format("{}_MeshParentTransformBuffer", debugName),
                    .ElementSize = sizeof(joint::MeshTransform),
                    .ElementCount = (uint32_t)meshCollection.MeshParentTransforms.size(),
                    .Flags = BufferFlag::StructuredBuffer,
                    .IsNeedStructuredBufferView = true,
                });
            }

            auto meshInstanceBuffer = std::make_unique<Buffer>(device, BufferCreation
            {
                .DebugName = std::format("{}_MeshInstanceBuffer", debugName),
                .ElementSize = sizeof(joint::MeshInstance),
                .ElementCount = (uint32_t)meshCollection.MeshInstances.size(),
                .Flags = BufferFlag::StructuredBuffer,
                .IsNeedStructuredBufferView = true,
            });

            auto materialBuffer = std::make_unique<Buffer>(device, BufferCreation
            {
                .DebugName = std::format("{}_MaterialBuffer", debugName),
                .ElementSize = sizeof(joint::Material),
                .ElementCount = (uint32_t)meshCollection.Materials.size(),
                .Flags = BufferFlag::StructuredBuffer,
                .IsNeedStructuredBufferView = true,
            });

            BenzinTrace("MeshCollectionGPUStorage created for '{}' mesh", debugName);
            BenzinTrace("VertexCount: {}, VertexSize: {}, VertexBufferSize: {}", totalVertexCount, sizeof(joint::MeshVertex), vertexBuffer->GetSizeInBytes());
            BenzinTrace("IndexCount: {}, IndexSize: {}, IndexBufferSize: {}", totalIndexCount, sizeof(uint32_t), indexBuffer->GetSizeInBytes());
            BenzinTrace("MeshInfoCount: {}, MeshInfoSize: {}, MeshInfoBufferSize: {}", meshCollection.Meshes.size(), sizeof(joint::MeshInfo), meshInfoBuffer->GetSizeInBytes());
            BenzinTraceIf(meshParentTransformBuffer, "MeshParentTransformCount: {}, MeshTransformSize: {}, MeshParentTransformBufferSize: {}", meshCollection.MeshParentTransforms.size(), sizeof(joint::MeshTransform), meshParentTransformBuffer->GetSizeInBytes());
            BenzinTrace("MeshInstanceCount: {}, MeshInstanceSize: {}, MeshInstanceBufferSize: {}", meshCollection.MeshInstances.size(), sizeof(joint::MeshInstance), meshInstanceBuffer->GetSizeInBytes());
            BenzinTrace("MaterialCount: {}, MaterialSize: {}, MaterialBufferSize: {}", meshCollection.Materials.size(), sizeof(joint::Material), materialBuffer->GetSizeInBytes());

            return MeshCollectionGPUStorage
            {
                .VertexBuffer = std::move(vertexBuffer),
                .IndexBuffer = std::move(indexBuffer),
                .MeshInfoBuffer = std::move(meshInfoBuffer),
                .MeshParentTransformBuffer = std::move(meshParentTransformBuffer),
                .MeshInstanceBuffer = std::move(meshInstanceBuffer),
                .MaterialBuffer = std::move(materialBuffer),
            };
        }

    } // anonymous namespace

    // Scene

    Scene::Scene(Device& device)
        : m_Device{ device }
    {
        m_EntityRegistry.on_construct<TransformComponent>().connect<&Scene::OnTransformComponentConstuct>(this);

        m_TopLevelASs.resize(GraphicsSettingsInstance::Get().FrameInFlightCount);

        m_CameraConstantBuffer = CreateFrameDependentConstantBuffer<joint::CameraConstants>(m_Device, "CameraConstantBuffer");
        m_PointLightBuffer = CreateFrameDependentUploadStructuredBuffer<joint::PointLight>(m_Device, "PointLightBuffer", g_MaxPointLightCount);
    }

    Scene::~Scene() = default;

    const rt::TopLevelAccelerationStructure& Scene::GetActiveTopLevelAS() const
    {
        return *m_TopLevelASs[m_Device.GetActiveFrameIndex()];
    }

    const Descriptor& Scene::GetCameraConstantBufferCBV() const
    {
        return m_CameraConstantBuffer->GetConstantBufferView(m_Device.GetActiveFrameIndex());
    }

    const Descriptor& Scene::GetPointLightBufferSRV() const
    {
        return m_PointLightBuffer->GetShaderResourceView(m_Device.GetActiveFrameIndex());
    }

    void Scene::OnUpdate(std::chrono::microseconds dt)
    {
        {
            const auto view = m_EntityRegistry.view<UpdateComponent>();
            for (const auto entityHandle : view)
            {
                auto& uc = view.get<UpdateComponent>(entityHandle);

                if (uc.Callback)
                {
                    uc.Callback(m_EntityRegistry, entityHandle, dt);
                }
            }
        }

        {
            const auto view = m_EntityRegistry.view<TransformComponent>();
            for (const auto entityHandle : view)
            {
                auto& tc = view.get<TransformComponent>(entityHandle);

                const DirectX::XMMATRIX worldMatrix = tc.GetWorldMatrix();
                const joint::MeshTransform transform
                {
                    .Matrix = worldMatrix,
                    .InverseMatrix = DirectX::XMMatrixInverse(nullptr, worldMatrix),
                };

                MappedData transformBuffer{ *tc.Buffer };
                transformBuffer.Write(transform, m_Device.GetActiveFrameIndex());
            }
        }

        {
            const joint::CameraConstants constants
            {
                .View = m_Camera.GetViewMatrix(),
                .InverseView = m_Camera.GetInverseViewMatrix(),
                .Projection = m_Camera.GetProjectionMatrix(),
                .InverseProjection = m_Camera.GetInverseProjectionMatrix(),
                .ViewProjection = m_Camera.GetViewProjectionMatrix(),
                .InverseViewProjection = m_Camera.GetInverseViewProjectionMatrix(),
                .InverseViewDirectionProjection = m_Camera.GetInverseViewDirectionProjectionMatrix(),
                .WorldPosition = *reinterpret_cast<const DirectX::XMFLOAT3*>(&m_Camera.GetPosition()),
            };

            MappedData cameraConstantBuffer{ *m_CameraConstantBuffer };
            cameraConstantBuffer.Write(constants, m_Device.GetActiveFrameIndex());
        }

        {
            const uint32_t offset = g_MaxPointLightCount * m_Device.GetActiveFrameIndex();
            MappedData pointLightBuffer{ *m_PointLightBuffer };

            const auto view = m_EntityRegistry.view<TransformComponent, PointLightComponent>();
            for (const auto [i, entityHandle] : view | std::views::enumerate)
            {
                const auto& tc = view.get<TransformComponent>(entityHandle);
                const auto& plc = view.get<PointLightComponent>(entityHandle);

                const joint::PointLight entry
                {
                    .Color = plc.Color,
                    .Intensity = plc.Intensity,
                    .WorldPosition = tc.Translation,
                    .ConstantAttenuation = 1.0f,
                    .LinearAttenuation = 4.5f / plc.Range,
                    .ExponentialAttenuation = 75.0f / (plc.Range * plc.Range),
                    .GeometryRadius = plc.GeometryRadius,
                };

                pointLightBuffer.Write(entry, offset + i);
            }

            m_Stats.PointLightCount = (uint32_t)view.size_hint();
        }
    }

    uint32_t Scene::PushMeshCollection(MeshCollectionResource&& meshCollectionResource)
    {
        BenzinAssert(!meshCollectionResource.DebugName.empty());
        BenzinAssert(!meshCollectionResource.Meshes.empty());
        BenzinAssert(!meshCollectionResource.MeshInstances.empty());
        BenzinAssert(!meshCollectionResource.Materials.empty());

        const auto textureOffset = (uint32_t)m_Textures.size();
        PushTextures(meshCollectionResource.TextureImages);

        const auto UpdateTextureIndexIfNeeded = [&](uint32_t& outTextureIndex)
        {
            if (outTextureIndex != g_InvalidIndex<uint32_t>)
            {
                outTextureIndex = m_Textures[textureOffset + outTextureIndex]->GetShaderResourceView().GetHeapIndex();
            }
        };

        for (auto& material : meshCollectionResource.Materials)
        {
            UpdateTextureIndexIfNeeded(material.AlbedoTextureIndex);
            UpdateTextureIndexIfNeeded(material.NormalTextureIndex);
            UpdateTextureIndexIfNeeded(material.MetalRoughnessTextureIndex);
            UpdateTextureIndexIfNeeded(material.EmissiveTextureIndex);
        }

        MeshUnion& meshUnion = m_MeshUnions.emplace_back();
        meshUnion.DebugName = std::move(meshCollectionResource.DebugName);
        meshUnion.Collection.Meshes = std::move(meshCollectionResource.Meshes);
        meshUnion.Collection.MeshParentTransforms = std::move(meshCollectionResource.MeshParentTransforms);
        meshUnion.Collection.MeshInstances = std::move(meshCollectionResource.MeshInstances);
        meshUnion.Collection.Materials = std::move(meshCollectionResource.Materials);
        meshUnion.GPUStorage = CreateMeshCollectionGPUStorage(m_Device, meshUnion.DebugName, meshUnion.Collection);

        PushBottomLevelAS(meshUnion);

        for (const auto& mesh : meshUnion.Collection.Meshes)
        {
            m_Stats.VertexCount += (uint32_t)mesh.Vertices.size();
            m_Stats.TriangleCount += (uint32_t)mesh.Indices.size() / 3;
        }

        return (uint32_t)m_MeshUnions.size() - 1;
    }

    void Scene::UploadMeshCollections()
    {
        UploadAllMeshData();
        UploadAllMeshParentTransforms();
        UploadAllMeshInstances();
        UploadAllTextures();
        UploadAllMaterials();
    }

    void Scene::BuildBottomLevelAccelerationStructures()
    {
        auto& graphicsCommandQueue = m_Device.GetGraphicsCommandQueue();
        auto& commandList = graphicsCommandQueue.GetCommandList();

        for (const auto& meshUnion : m_MeshUnions)
        {
            for (const auto& blas : meshUnion.BottomLevelASs)
            {
                commandList.SetResourceBarrier(TransitionBarrier{ blas->GetScratchResource(), ResourceState::UnorderedAccess });
                commandList.BuildRayTracingAccelerationStructure(*blas);
            }
        }

        // Wait for BottomLevelASs
        for (const auto& meshUnion : m_MeshUnions)
        {
            for (const auto& blas : meshUnion.BottomLevelASs)
            {
                commandList.SetResourceBarrier(UnorderedAccessBarrier{ blas->GetBuffer() });
            }
        }
    }

    void Scene::BuildTopLevelAccelerationStructure()
    {
        CreateTopLevelAS();

        auto& graphicsCommandQueue = m_Device.GetGraphicsCommandQueue();
        auto& commandList = graphicsCommandQueue.GetCommandList();

        auto& activeTopLevelAS = GetActiveTopLevelAS();
        commandList.SetResourceBarrier(TransitionBarrier{ activeTopLevelAS->GetScratchResource(), ResourceState::UnorderedAccess });
        commandList.BuildRayTracingAccelerationStructure(*activeTopLevelAS);
    }

    std::unique_ptr<rt::TopLevelAccelerationStructure>& Scene::GetActiveTopLevelAS()
    {
        return m_TopLevelASs[m_Device.GetActiveFrameIndex()];
    }

    void Scene::OnTransformComponentConstuct(entt::registry& registry, entt::entity entityHandle)
    {
        auto& tc = registry.get<TransformComponent>(entityHandle);
        tc.Buffer = CreateFrameDependentConstantBuffer<joint::MeshTransform>(m_Device, std::format("TransformBuffer_{}", magic_enum::enum_integer(entityHandle)));
    }

    void Scene::PushTextures(std::span<const TextureImage> textureImages)
    {
        if (textureImages.empty())
        {
            return;
        }

        m_TexturesData.reserve(m_TexturesData.size() + textureImages.size());
        m_Textures.reserve(m_Textures.size() + textureImages.size());

        for (const auto& textureImage : textureImages)
        {
            m_TexturesData.push_back(std::move(textureImage.ImageData));

            m_Textures.push_back(std::make_unique<Texture>(m_Device, TextureCreation
            {
                .DebugName = textureImage.DebugName,
                .Type = TextureType::Texture2D,
                .Format = textureImage.Format,
                .Width = textureImage.Width,
                .Height = textureImage.Height,
                .MipCount = 1, // #TODO: Mip generation
                .IsNeedShaderResourceView = true,
            }));
        }
    }

    void Scene::PushBottomLevelAS(MeshUnion& meshUnion)
    {
        uint32_t vertexOffset = 0;
        uint32_t indexOffset = 0;
        for (const auto& mesh : meshUnion.Collection.Meshes)
        {
            const auto vertexCount = (uint32_t)mesh.Vertices.size();
            const auto indexCount = (uint32_t)mesh.Indices.size();

            const rt::GeometryVariant meshGeometryDesc = rt::TriangledGeometry
            {
                .VertexBuffer = *meshUnion.GPUStorage.VertexBuffer,
                .IndexBuffer = *meshUnion.GPUStorage.IndexBuffer,
                .VertexOffset = vertexOffset,
                .IndexOffset = indexOffset,
                .VertexCount = vertexCount,
                .IndexCount = indexCount,
            };

            meshUnion.BottomLevelASs.push_back(std::make_unique<rt::BottomLevelAccelerationStructure>(m_Device, rt::BottomLevelAccelerationStructureCreation
            {
                .DebugName = meshUnion.DebugName,
                .Geometries = std::span{ &meshGeometryDesc, 1 },
            }));

            vertexOffset += vertexCount;
            indexOffset += indexCount;
        }
    }

    void Scene::CreateTopLevelAS()
    {
        std::vector<rt::TopLevelInstance> topLevelInstances;

        const auto view = m_EntityRegistry.view<TransformComponent, MeshInstanceComponent>(entt::exclude<PointLightComponent>);
        for (const auto entityHandle : view)
        {
            const auto& tc = view.get<TransformComponent>(entityHandle);
            const auto& mic = view.get<MeshInstanceComponent>(entityHandle);

            const auto& meshUnion = m_MeshUnions[mic.MeshUnionIndex];
            const auto& meshCollection = meshUnion.Collection;

            const auto meshInstanceRange = mic.MeshInstanceRange.value_or(meshCollection.GetFullMeshInstanceRange());
            for (const auto i : IndexRangeToView(meshInstanceRange))
            {
                const auto& meshInstance = meshCollection.MeshInstances[i];

                topLevelInstances.push_back(rt::TopLevelInstance
                {
                    .BottomLevelAccelerationStructure = *meshUnion.BottomLevelASs[meshInstance.MeshIndex],
                    .HitGroupIndex = 0,
                    .Transform = meshCollection.GetMeshParentTransform(meshInstance.MeshParentTransformIndex) * tc.GetWorldMatrix(),
                });
            }
        }

        GetActiveTopLevelAS() = std::make_unique<rt::TopLevelAccelerationStructure>(m_Device, rt::TopLevelAccelerationStructureCreation
        {
            .DebugName = "SceneTopLevelAS",
            .Instances = topLevelInstances,
        });
    }

    void Scene::UploadAllMeshData()
    {
        uint64_t uploadBufferSize = 0;
        for (const auto& meshUnion : m_MeshUnions)
        {
            uploadBufferSize += meshUnion.GPUStorage.VertexBuffer->GetSizeInBytes();
            uploadBufferSize += meshUnion.GPUStorage.IndexBuffer->GetSizeInBytes();
            uploadBufferSize += meshUnion.GPUStorage.MeshInfoBuffer->GetSizeInBytes();
        }

        auto& copyCommandQueue = m_Device.GetCopyCommandQueue();
        BenzinFlushCommandQueueOnScopeExit(copyCommandQueue);

        auto& copyCommandList = copyCommandQueue.GetCommandList(uploadBufferSize);

        for (const auto& meshUnion : m_MeshUnions)
        {
            uint32_t vertexOffset = 0;
            uint32_t indexOffset = 0;
            for (const auto [i, mesh] : meshUnion.Collection.Meshes | std::views::enumerate)
            {
                const joint::MeshInfo meshInfo
                {
                    .VertexOffset = vertexOffset,
                    .IndexOffset = indexOffset,
                };

                copyCommandList.UpdateBuffer(*meshUnion.GPUStorage.VertexBuffer, std::span<const joint::MeshVertex>{ mesh.Vertices }, vertexOffset);
                copyCommandList.UpdateBuffer(*meshUnion.GPUStorage.IndexBuffer, std::span<const uint32_t>{ mesh.Indices }, indexOffset);
                copyCommandList.UpdateBuffer(*meshUnion.GPUStorage.MeshInfoBuffer, std::span{ &meshInfo, 1 }, i);

                vertexOffset += (uint32_t)mesh.Vertices.size();
                indexOffset += (uint32_t)mesh.Indices.size();
            }
        }
    }

    void Scene::UploadAllMeshParentTransforms()
    {
        uint64_t uploadBufferSize = 0;
        for (const auto& meshUnion : m_MeshUnions)
        {
            if (!meshUnion.GPUStorage.MeshParentTransformBuffer)
            {
                break;
            }

            uploadBufferSize += meshUnion.GPUStorage.MeshParentTransformBuffer->GetSizeInBytes();
        }

        auto& copyCommandQueue = m_Device.GetCopyCommandQueue();
        BenzinFlushCommandQueueOnScopeExit(copyCommandQueue);

        auto& copyCommandList = copyCommandQueue.GetCommandList(uploadBufferSize);

        for (const auto& meshUnion : m_MeshUnions)
        {
            if (!meshUnion.GPUStorage.MeshParentTransformBuffer)
            {
                break;
            }

            for (const auto& [i, parentTransform] : meshUnion.Collection.MeshParentTransforms | std::views::enumerate)
            {
                const joint::MeshTransform transform 
                {
                    .Matrix = parentTransform,
                    .InverseMatrix = DirectX::XMMatrixInverse(nullptr, parentTransform),
                };
                copyCommandList.UpdateBuffer(*meshUnion.GPUStorage.MeshParentTransformBuffer, std::span{ &transform, 1 }, i);
            }
        }
    }

    void Scene::UploadAllMeshInstances()
    {
        uint64_t uploadBufferSize = 0;
        for (const auto& meshUnion : m_MeshUnions)
        {
            uploadBufferSize += meshUnion.GPUStorage.MeshInstanceBuffer->GetSizeInBytes();
        }

        auto& copyCommandQueue = m_Device.GetCopyCommandQueue();
        BenzinFlushCommandQueueOnScopeExit(copyCommandQueue);

        auto& copyCommandList = copyCommandQueue.GetCommandList(uploadBufferSize);

        for (const auto& meshUnion : m_MeshUnions)
        {
            for (const auto& [i, meshInstance] : meshUnion.Collection.MeshInstances | std::views::enumerate)
            {
                copyCommandList.UpdateBuffer(*meshUnion.GPUStorage.MeshInstanceBuffer, std::span<const MeshInstance>{ &meshInstance, 1 }, i);
            }
        }
    }

    void Scene::UploadAllTextures()
    {
        uint64_t uploadBufferSize = 0;
        for (const auto& texture : m_Textures)
        {
            uploadBufferSize += AlignAbove(texture->GetSizeInBytes(), config::g_TextureAlignment);
        }

        auto& copyCommandQueue = m_Device.GetCopyCommandQueue();
        BenzinFlushCommandQueueOnScopeExit(copyCommandQueue);

        auto& copyCommandList = copyCommandQueue.GetCommandList(uploadBufferSize);
        for (const auto& [textureData, texture] : std::views::zip(m_TexturesData, m_Textures))
        {
            copyCommandList.UpdateTextureTopMip(*texture, textureData);
        }
    }

    void Scene::UploadAllMaterials()
    {
        uint64_t uploadBufferSize = 0;
        for (const auto& meshUnion : m_MeshUnions)
        {
            uploadBufferSize += meshUnion.GPUStorage.MaterialBuffer->GetSizeInBytes();
        }

        auto& copyCommandQueue = m_Device.GetCopyCommandQueue();
        BenzinFlushCommandQueueOnScopeExit(copyCommandQueue);

        auto& copyCommandList = copyCommandQueue.GetCommandList(uploadBufferSize);

        for (const auto& meshUnion : m_MeshUnions)
        {
            copyCommandList.UpdateBuffer(*meshUnion.GPUStorage.MaterialBuffer, std::span<const Material>{ meshUnion.Collection.Materials });
        }
    }

} // namespace benzin
