#include "benzin/config/bootstrap.hpp"
#include "benzin/engine/scene.hpp"

#include <shaders/joint/structured_buffer_types.hpp>

#include "benzin/core/asserter.hpp"
#include "benzin/core/engine_math.hpp"
#include "benzin/core/logger.hpp"
#include "benzin/core/math.hpp"
#include "benzin/engine/entity_components.hpp"
#include "benzin/graphics/buffer.hpp"
#include "benzin/graphics/command_queue.hpp"
#include "benzin/graphics/device.hpp"
#include "benzin/graphics/texture.hpp"

namespace benzin
{

    static constexpr uint32_t g_MaxPointLightCount = 200;

    static MeshCollectionGpuStorage CreateMeshCollectionGpuStorage(Device& device, std::string_view debugName, const MeshCollection& meshCollection)
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
            .Type = BufferType::Structured,
            .ElementSize = sizeof(joint::MeshVertex),
            .ElementCount = (uint32_t)totalVertexCount,
        });

        auto indexBuffer = std::make_unique<Buffer>(device, BufferCreation
        {
            .DebugName = std::format("{}_IndexBuffer", debugName),
            .Type = BufferType::Format,
            .Format = GraphicsFormat::R32Uint,
            .ElementSize = sizeof(uint32_t),
            .ElementCount = (uint32_t)totalIndexCount,
        });

        auto meshInfoBuffer = std::make_unique<Buffer>(device, BufferCreation
        {
            .DebugName = std::format("{}_MeshInfoBuffer", debugName),
            .Type = BufferType::Structured,
            .ElementSize = sizeof(joint::MeshInfo),
            .ElementCount = (uint32_t)meshCollection.Meshes.size(),
        });

        auto meshInstanceBuffer = std::make_unique<Buffer>(device, BufferCreation
        {
            .DebugName = std::format("{}_MeshInstanceBuffer", debugName),
            .Type = BufferType::Structured,
            .ElementSize = sizeof(joint::MeshInstance),
            .ElementCount = (uint32_t)meshCollection.MeshInstances.size(),
        });

        auto materialBuffer = std::make_unique<Buffer>(device, BufferCreation
        {
            .DebugName = std::format("{}_MaterialBuffer", debugName),
            .Type = BufferType::Structured,
            .ElementSize = sizeof(joint::Material),
            .ElementCount = (uint32_t)meshCollection.Materials.size(),
        });

        return MeshCollectionGpuStorage
        {
            .VertexBuffer = std::move(vertexBuffer),
            .IndexBuffer = std::move(indexBuffer),
            .MeshInfoBuffer = std::move(meshInfoBuffer),
            .MeshInstanceBuffer = std::move(meshInstanceBuffer),
            .MaterialBuffer = std::move(materialBuffer),
        };
    }

    // Scene

    Scene::Scene(Device& device)
        : m_Device{ device }
    {
        m_EntityRegistry.on_construct<TransformComponent>().connect<&Scene::OnTransformComponentConstuct>(this);

        MakeUniquePtr(m_PointLightBuffer, m_Device, BufferCreation
        {
            .DebugName = "PointLightBuffer",
            .MemoryType = ResourceMemoryType::Upload,
            .Type = BufferType::Structured,
            .ElementSize = sizeof(joint::PointLight),
            .ElementCount = g_MaxPointLightCount * CommandLineArgs::GetU32("FrameInFlightCount"),
        });
    }

    Scene::~Scene() = default;

    std::string_view Scene::GetMeshCollectionDebugName(entt::entity meshHandle) const
    {
        return m_MeshRegistry.get<std::string>(meshHandle);
    }

    const MeshCollection& Scene::GetMeshCollection(entt::entity meshHandle) const
    {
        return m_MeshRegistry.get<MeshCollection>(meshHandle);
    }

    const MeshCollectionGpuStorage& Scene::GetMeshCollectionGpuStorage(entt::entity meshHandle) const
    {
        return m_MeshRegistry.get<MeshCollectionGpuStorage>(meshHandle);
    }

    const Descriptor& Scene::GetPointLightBufferStructuredSrv() const
    {
        return m_PointLightBuffer->GetSrv(IndexRange32
        {
            .StartIndex = m_Device.GetActiveFrameIndex() * g_MaxPointLightCount,
            .Count = g_MaxPointLightCount,
        });
    }

    void Scene::OnUpdate()
    {
        {
            const auto view = m_EntityRegistry.view<UpdateComponent>();
            for (const auto& [entityHandle, uc] : view.each())
            {
                BenzinAssert((bool)uc.Callback);
                uc.Callback(m_EntityRegistry, entityHandle);
            }
        }

        {
            const auto view = m_EntityRegistry.view<TransformComponent>();
            for (const auto& [entityHandle, tc] : view.each())
            {
                tc.UpdateTransformConstantBuffer();
            }
        }

        {
            const uint32_t offset = g_MaxPointLightCount * m_Device.GetActiveFrameIndex();
            const MemoryWriter writer{ m_PointLightBuffer->GetCpuMappedData(), m_PointLightBuffer->GetSize() };

            const auto view = m_EntityRegistry.view<TransformComponent, PointLightComponent>();
            for (const auto [i, entityHandle] : view | std::views::enumerate)
            {
                const auto& tc = view.get<TransformComponent>(entityHandle);
                const auto& plc = view.get<PointLightComponent>(entityHandle);

                const joint::PointLight entry
                {
                    .Color = plc.Color,
                    .Intensity = plc.Intensity,
                    .WorldPosition = tc.GetTranslation(),
                    .ConstantAttenuation = 1.0f,
                    .LinearAttenuation = 4.5f / plc.Range,
                    .ExponentialAttenuation = 75.0f / (plc.Range * plc.Range),
                    .GeometryRadius = plc.GeometryRadius,
                };

                writer.Write(entry, offset + i);
            }
        }
    }

    entt::entity Scene::PushMeshCollection(MeshCollectionResource&& meshCollectionResource)
    {
        BenzinAssert(!meshCollectionResource.DebugName.empty());
        BenzinAssert(!meshCollectionResource.Meshes.empty());
        BenzinAssert(!meshCollectionResource.MeshInstances.empty());
        BenzinAssert(!meshCollectionResource.Materials.empty());

        const auto textureOffset = (uint32_t)m_Textures.size();
        PushTextures(meshCollectionResource.TextureImages);

        const auto UpdateTextureIndexIfNeeded = [&](uint32_t& outTextureIndex)
        {
            if (IsValidUnsigned(outTextureIndex))
            {
                outTextureIndex = m_Textures[textureOffset + outTextureIndex]->GetSrv().GetGpuHeapIndex();
            }
        };

        for (auto& material : meshCollectionResource.Materials)
        {
            UpdateTextureIndexIfNeeded(material.AlbedoTextureIndex);
            UpdateTextureIndexIfNeeded(material.NormalTextureIndex);
            UpdateTextureIndexIfNeeded(material.MetallicRoughnessTextureIndex);
            UpdateTextureIndexIfNeeded(material.EmissiveTextureIndex);
        }

        const entt::entity meshHandle = m_MeshRegistry.create();

        auto& meshDebugName = m_MeshRegistry.emplace<std::string>(meshHandle);
        meshDebugName = std::move(meshCollectionResource.DebugName);

        auto& meshCollection = m_MeshRegistry.emplace<MeshCollection>(meshHandle);
        meshCollection.Meshes = std::move(meshCollectionResource.Meshes);
        meshCollection.MeshInstances = std::move(meshCollectionResource.MeshInstances);
        meshCollection.Materials = std::move(meshCollectionResource.Materials);

        auto& meshGpuStorage = m_MeshRegistry.emplace<MeshCollectionGpuStorage>(meshHandle);
        meshGpuStorage = CreateMeshCollectionGpuStorage(m_Device, meshDebugName, meshCollection);

        UpdateStats(meshHandle);

        return meshHandle;
    }

    void Scene::UploadMeshCollectionsToGpu()
    {
        BenzinLogTimeOnScopeExit("Scene::UploadMeshCollectionsToGpu");

        UploadAllMeshData();
        UploadAllMeshInstances();
        UploadAllTextures();
        UploadAllMaterials();
    }

    void Scene::OnTransformComponentConstuct(entt::registry& registry, entt::entity entityHandle)
    {
        auto& tc = registry.get<TransformComponent>(entityHandle);
        tc.CreateTransformConstantBuffer(m_Device, std::format("TransformBuffer_{}", magic_enum::enum_integer(entityHandle)));
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
                .Format = textureImage.Format,
                .Width = textureImage.Width,
                .Height = textureImage.Height,
                .MipCount = 1, // #TODO: Mip generation
            }));
        }
    }

    void Scene::UploadAllMeshData()
    {
        Bytes32 uploadBufferSize;
        m_MeshRegistry.each([this, &uploadBufferSize](entt::entity meshHandle)
        {
            const auto& gpuStorage = m_MeshRegistry.get<MeshCollectionGpuStorage>(meshHandle);

            uploadBufferSize += gpuStorage.VertexBuffer->GetSize();
            uploadBufferSize += gpuStorage.IndexBuffer->GetSize();
            uploadBufferSize += gpuStorage.MeshInfoBuffer->GetSize();
        });

        auto& commandList = m_Device.GetGraphicsCommandQueue().GetCommandList(uploadBufferSize);
        m_MeshRegistry.each([this, &commandList](entt::entity meshHandle)
        {
            const auto& gpuStorage = m_MeshRegistry.get<MeshCollectionGpuStorage>(meshHandle);
            auto& meshCollection = m_MeshRegistry.get<MeshCollection>(meshHandle);

            uint32_t vertexOffset = 0;
            uint32_t indexOffset = 0;
            for (const auto [i, mesh] : meshCollection.Meshes | std::views::enumerate)
            {
                const joint::MeshInfo meshInfo
                {
                    .VertexOffset = vertexOffset,
                    .IndexOffset = indexOffset,
                };

                meshCollection.MeshInfos.push_back(meshInfo);

                commandList.UploadToBuffer<joint::MeshVertex>(*gpuStorage.VertexBuffer, mesh.Vertices, vertexOffset);
                commandList.UploadToBuffer<uint32_t>(*gpuStorage.IndexBuffer, mesh.Indices, indexOffset);
                commandList.UploadToBuffer(*gpuStorage.MeshInfoBuffer, ToSingleSpan(meshInfo), i);

                vertexOffset += (uint32_t)mesh.Vertices.size();
                indexOffset += (uint32_t)mesh.Indices.size();
            }
        });
    }

    void Scene::UploadAllMeshInstances()
    {
        Bytes32 uploadBufferSize;
        m_MeshRegistry.each([this, &uploadBufferSize](entt::entity meshHandle)
        {
            const auto& gpuStorage = m_MeshRegistry.get<MeshCollectionGpuStorage>(meshHandle);

            uploadBufferSize += gpuStorage.MeshInstanceBuffer->GetSize();
        });

        auto& commandList = m_Device.GetGraphicsCommandQueue().GetCommandList(uploadBufferSize);
        m_MeshRegistry.each([this, &commandList](entt::entity meshHandle)
        {
            const auto& meshCollection = m_MeshRegistry.get<MeshCollection>(meshHandle);
            const auto& gpuStorage = m_MeshRegistry.get<MeshCollectionGpuStorage>(meshHandle);

            for (const auto& [i, meshInstance] : meshCollection.MeshInstances | std::views::enumerate)
            {
                const joint::MeshInstance gpuMeshInstance
                {
                    .Transform = meshInstance.Transform,
                    .MeshIndex = meshInstance.MeshIndex,
                    .MaterialIndex = meshInstance.MaterialIndex,
                };

                commandList.UploadToBuffer(*gpuStorage.MeshInstanceBuffer, ToSingleSpan(gpuMeshInstance), i);
            }
        });
    }

    void Scene::UploadAllTextures()
    {
        if (m_Textures.empty())
        {
            return;
        }

        Bytes32 uploadBufferSize;
        for (const auto& texture : m_Textures)
        {
            uploadBufferSize += Bytes32{ AlignUp(texture->GetSize().GetByteCount(), GfxConfig::s_TextureAlignment.GetByteCount()) };
        }

        auto& commandList = m_Device.GetGraphicsCommandQueue().GetCommandList(uploadBufferSize);

        for (const auto& [textureData, texture] : std::views::zip(m_TexturesData, m_Textures))
        {
            commandList.UploadToTextureTopMip(*texture, textureData);
        }
    }

    void Scene::UploadAllMaterials()
    {
        static_assert(sizeof(Material) == sizeof(joint::Material));

        Bytes32 uploadBufferSize;
        m_MeshRegistry.each([this, &uploadBufferSize](entt::entity meshHandle)
        {
            const auto& gpuStorage = m_MeshRegistry.get<MeshCollectionGpuStorage>(meshHandle);

            uploadBufferSize += gpuStorage.MaterialBuffer->GetSize();
        });

        auto& commandList = m_Device.GetGraphicsCommandQueue().GetCommandList(uploadBufferSize);
        m_MeshRegistry.each([this, &commandList](entt::entity meshHandle)
        {
            const auto& meshCollection = m_MeshRegistry.get<MeshCollection>(meshHandle);
            const auto& gpuStorage = m_MeshRegistry.get<MeshCollectionGpuStorage>(meshHandle);

            commandList.UploadToBuffer<Material>(*gpuStorage.MaterialBuffer, meshCollection.Materials);
        });
    }

    void Scene::UpdateStats(entt::entity meshHandle)
    {
        const auto& meshCollection = m_MeshRegistry.get<MeshCollection>(meshHandle);

        for (const auto& mesh : meshCollection.Meshes)
        {
            m_Stats.VertexCount += (uint32_t)mesh.Vertices.size();
            m_Stats.TriangleCount += (uint32_t)mesh.Indices.size() / 3;

            ++m_Stats.MeshCount;
        }

        m_Stats.MaterialCount += (uint32_t)meshCollection.Materials.size();
        m_Stats.MeshInstanceCount += (uint32_t)meshCollection.MeshInstances.size();
    }

}
