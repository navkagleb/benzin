#include "benzin/config/bootstrap.hpp"
#include "benzin/engine/scene.hpp"

#include "benzin/engine/entity_components.hpp"
#include "benzin/graphics/buffer.hpp"
#include "benzin/graphics/command_queue.hpp"
#include "benzin/graphics/device.hpp"
#include "benzin/graphics/mapped_data.hpp"
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

            // #TODO: Add StructuredBuffer flag
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
                .ElementSize = sizeof(uint32_t),
                .ElementCount = (uint32_t)totalIndexCount,
            });
            indexBuffer->PushFormatBufferView({ .Format = GraphicsFormat::R32Uint });

            auto meshInfoBuffer = std::make_unique<Buffer>(device, BufferCreation
            {
                .DebugName = std::format("{}_MeshInfoBuffer", debugName),
                .ElementSize = sizeof(joint::MeshInfo),
                .ElementCount = (uint32_t)meshCollection.Meshes.size(),
                .Flags = BufferFlag::StructuredBuffer,
                .IsNeedStructuredBufferView = true,
            });

            std::unique_ptr<Buffer> meshNodeBuffer;
            if (!meshCollection.MeshNodes.empty())
            {
                meshNodeBuffer = std::make_unique<Buffer>(device, BufferCreation
                {
                    .DebugName = std::format("{}_MeshNodeBuffer", debugName),
                    .ElementSize = sizeof(joint::MeshNode),
                    .ElementCount = (uint32_t)meshCollection.MeshNodes.size(),
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
            BenzinTraceIf(meshNodeBuffer, "MeshNodeCount: {}, MeshNodeSize: {}, MeshNodeBufferSize: {}", meshCollection.MeshNodes.size(), sizeof(joint::MeshNode), meshNodeBuffer->GetSizeInBytes());
            BenzinTrace("MeshInstanceCount: {}, MeshInstanceSize: {}, MeshInstanceBufferSize: {}", meshCollection.MeshInstances.size(), sizeof(joint::MeshInstance), meshInstanceBuffer->GetSizeInBytes());
            BenzinTrace("MaterialCount: {}, MaterialSize: {}, MaterialBufferSize: {}", meshCollection.Materials.size(), sizeof(joint::Material), materialBuffer->GetSizeInBytes());

            return MeshCollectionGPUStorage
            {
                .VertexBuffer = std::move(vertexBuffer),
                .IndexBuffer = std::move(indexBuffer),
                .MeshInfoBuffer = std::move(meshInfoBuffer),
                .MeshNodeBuffer = std::move(meshNodeBuffer),
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

        m_PointLightBuffer = CreateFrameDependentUploadStructuredBuffer<joint::PointLight>(m_Device, "PointLightBuffer", g_MaxPointLightCount);
    }

    void Scene::OnUpdate()
    {
        {
            const auto view = m_EntityRegistry.view<TransformComponent>();
            for (const auto entityHandle : view)
            {
                auto& tc = view.get<TransformComponent>(entityHandle);

                const DirectX::XMMATRIX worldMatrix = tc.GetMatrix();
                const joint::Transform transform
                {
                    .WorldMatrix = worldMatrix,
                    .InverseWorldMatrix = DirectX::XMMatrixInverse(nullptr, worldMatrix),
                };

                MappedData transformBuffer{ *tc.Buffer };
                transformBuffer.Write(transform, m_Device.GetActiveFrameIndex());
            }
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
                };

                pointLightBuffer.Write(entry, offset + i);
            }

            m_ActivePointLightCount = (uint32_t)view.size_hint();
        }
    }

    uint32_t Scene::PushMeshCollection(MeshCollectionResource&& meshCollectionResource)
    {
        BenzinAssert(!meshCollectionResource.DebugName.empty());
        BenzinAssert(!meshCollectionResource.Meshes.empty());
        BenzinAssert(!meshCollectionResource.MeshInstances.empty());
        BenzinAssert(!meshCollectionResource.Materials.empty());

        const auto textureOffset = (uint32_t)m_Textures.size();
        PushAndUploadTextures(meshCollectionResource.TextureImages);

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

        MeshCollection meshCollection
        {
            .Meshes = std::move(meshCollectionResource.Meshes),
            .MeshNodes = std::move(meshCollectionResource.MeshNodes),
            .MeshInstances = std::move(meshCollectionResource.MeshInstances),
            .Materials = std::move(meshCollectionResource.Materials),
        };

        m_MeshCollectionDebugNames.push_back(std::move(meshCollectionResource.DebugName));
        m_MeshCollections.push_back(std::move(meshCollection));
        m_MeshCollectionGPUStorages.push_back(CreateMeshCollectionGPUStorage(m_Device, m_MeshCollectionDebugNames.back(), m_MeshCollections.back()));

        return (uint32_t)m_MeshCollections.size() - 1;
    }

    void Scene::UploadMeshCollections()
    {
        UploadAllMeshData();
        UploadAllMeshNodes();
        UploadAllMeshInstances();
        UploadAllTextures();
        UploadAllMaterials();
    }

    void Scene::OnTransformComponentConstuct(entt::registry& registry, entt::entity entityHandle)
    {
        auto& tc = registry.get<TransformComponent>(entityHandle);
        tc.Buffer = CreateFrameDependentConstantBuffer<joint::Transform>(m_Device, std::format("TransformBuffer_{}", magic_enum::enum_integer(entityHandle)));
    }

    void Scene::PushAndUploadTextures(std::span<const TextureImage> textureImages)
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

    void Scene::UploadAllMeshData()
    {
        uint64_t uploadBufferSize = 0;
        for (const auto& gpuStorage : m_MeshCollectionGPUStorages)
        {
            uploadBufferSize += gpuStorage.VertexBuffer->GetSizeInBytes();
            uploadBufferSize += gpuStorage.IndexBuffer->GetSizeInBytes();
            uploadBufferSize += gpuStorage.MeshInfoBuffer->GetSizeInBytes();
        }

        auto& copyCommandQueue = m_Device.GetCopyCommandQueue();
        BenzinFlushCommandQueueOnScopeExit(copyCommandQueue);

        auto& copyCommandList = copyCommandQueue.GetCommandList(uploadBufferSize);

        for (const auto& [gpuStorage, meshCollection] : std::views::zip(m_MeshCollectionGPUStorages, m_MeshCollections))
        {
            uint32_t vertexOffset = 0;
            uint32_t indexOffset = 0;
            for (const auto [i, mesh] : meshCollection.Meshes | std::views::enumerate)
            {
                const joint::MeshInfo meshInfo
                {
                    .VertexOffset = vertexOffset,
                    .IndexOffset = indexOffset,
                };

                copyCommandList.UpdateBuffer(*gpuStorage.VertexBuffer, std::span<const joint::MeshVertex>{ mesh.Vertices }, vertexOffset);
                copyCommandList.UpdateBuffer(*gpuStorage.IndexBuffer, std::span<const uint32_t>{ mesh.Indices }, indexOffset);
                copyCommandList.UpdateBuffer(*gpuStorage.MeshInfoBuffer, std::span{ &meshInfo, 1 }, i);

                vertexOffset += (uint32_t)mesh.Vertices.size();
                indexOffset += (uint32_t)mesh.Indices.size();
            }
        }
    }

    void Scene::UploadAllMeshNodes()
    {
        uint64_t uploadBufferSize = 0;
        for (const auto& gpuStorage : m_MeshCollectionGPUStorages)
        {
            if (!gpuStorage.MeshNodeBuffer)
            {
                break;
            }

            uploadBufferSize += gpuStorage.MeshNodeBuffer->GetSizeInBytes();
        }

        auto& copyCommandQueue = m_Device.GetCopyCommandQueue();
        BenzinFlushCommandQueueOnScopeExit(copyCommandQueue);

        auto& copyCommandList = copyCommandQueue.GetCommandList(uploadBufferSize);

        for (const auto& [gpuStorage, meshCollection] : std::views::zip(m_MeshCollectionGPUStorages, m_MeshCollections))
        {
            if (!gpuStorage.MeshNodeBuffer)
            {
                break;
            }

            for (const auto& [i, meshNode] : meshCollection.MeshNodes | std::views::enumerate)
            {
                // #TODO: Rename
                const joint::MeshNode entry 
                {
                    .WorldTransform = meshNode.Transform,
                    .InverseWorldTransform = DirectX::XMMatrixInverse(nullptr, meshNode.Transform),
                };
                copyCommandList.UpdateBuffer(*gpuStorage.MeshNodeBuffer, std::span{ &entry, 1 }, i);
            }
        }
    }

    void Scene::UploadAllMeshInstances()
    {
        uint64_t uploadBufferSize = 0;
        for (const auto& gpuStorage : m_MeshCollectionGPUStorages)
        {
            uploadBufferSize += gpuStorage.MeshInstanceBuffer->GetSizeInBytes();
        }

        auto& copyCommandQueue = m_Device.GetCopyCommandQueue();
        BenzinFlushCommandQueueOnScopeExit(copyCommandQueue);

        auto& copyCommandList = copyCommandQueue.GetCommandList(uploadBufferSize);

        for (const auto& [gpuStorage, meshCollection] : std::views::zip(m_MeshCollectionGPUStorages, m_MeshCollections))
        {
            for (const auto& [i, meshInstance] : meshCollection.MeshInstances | std::views::enumerate)
            {
                copyCommandList.UpdateBuffer(*gpuStorage.MeshInstanceBuffer, std::span<const MeshInstance>{ &meshInstance, 1 }, i);
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
        for (const auto& gpuStorage : m_MeshCollectionGPUStorages)
        {
            uploadBufferSize += gpuStorage.MaterialBuffer->GetSizeInBytes();
        }

        auto& copyCommandQueue = m_Device.GetCopyCommandQueue();
        BenzinFlushCommandQueueOnScopeExit(copyCommandQueue);

        auto& copyCommandList = copyCommandQueue.GetCommandList(uploadBufferSize);

        for (const auto& [gpuStorage, meshCollection] : std::views::zip(m_MeshCollectionGPUStorages, m_MeshCollections))
        {
            copyCommandList.UpdateBuffer(*gpuStorage.MaterialBuffer, std::span<const Material>{ meshCollection.Materials });
        }
    }

} // namespace benzin
