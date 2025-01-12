#include "benzin/config/bootstrap.hpp"
#include "benzin/engine/scene.hpp"

#include <shaders/joint/mesh_types.hpp>
#include <shaders/joint/structured_buffer_types.hpp> // TODO: Remove

#include "benzin/core/asserter.hpp"
#include "benzin/core/engine_math.hpp"
#include "benzin/core/logger.hpp"
#include "benzin/core/math.hpp"
#include "benzin/engine/entity_components.hpp"
#include "benzin/engine/mesh.hpp"
#include "benzin/engine/resource_loader.hpp"
#include "benzin/graphics/buffer.hpp"
#include "benzin/graphics/command_queue.hpp"
#include "benzin/graphics/device.hpp"
#include "benzin/graphics/texture.hpp"

namespace benzin
{

    static constexpr uint32_t g_MaxPointLightCount = 200;

    static MeshGpuStorage CreateMeshGpuStorage(Device& device, std::string_view debugName, const Mesh& mesh)
    {
        MeshGpuStorage meshGpuStorage;

        MakeUniquePtr(meshGpuStorage.VertexBuffer, device, BufferCreation
        {
            .DebugName = std::format("{}_VertexBuffer", debugName),
            .Type = BufferType::Structured,
            .ElementSize = sizeof(joint::MeshVertex),
            .ElementCount = (uint32_t)mesh.TotalVertexCount,
        });

        MakeUniquePtr(meshGpuStorage.IndexBuffer, device, BufferCreation
        {
            .DebugName = std::format("{}_IndexBuffer", debugName),
            .Type = BufferType::Format,
            .Format = GraphicsFormat::R32Uint,
            .ElementSize = sizeof(uint32_t),
            .ElementCount = (uint32_t)mesh.TotalIndexCount,
        });

        MakeUniquePtr(meshGpuStorage.MeshInfoBuffer, device, BufferCreation
        {
            .DebugName = std::format("{}_MeshInfoBuffer", debugName),
            .Type = BufferType::Structured,
            .ElementSize = sizeof(joint::MeshInfo),
            .ElementCount = (uint32_t)mesh.SubMeshes.size(),
        });

        MakeUniquePtr(meshGpuStorage.MeshInstanceBuffer, device, BufferCreation
        {
            .DebugName = std::format("{}_MeshInstanceBuffer", debugName),
            .Type = BufferType::Structured,
            .ElementSize = sizeof(joint::MeshInstance),
            .ElementCount = (uint32_t)mesh.SubMeshInstances.size(),
        });

        MakeUniquePtr(meshGpuStorage.MaterialBuffer, device, BufferCreation
        {
            .DebugName = std::format("{}_MaterialBuffer", debugName),
            .Type = BufferType::Structured,
            .ElementSize = sizeof(joint::Material),
            .ElementCount = (uint32_t)mesh.Materials.size(),
        });

        return meshGpuStorage;
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

    entt::entity Scene::AddMesh(MeshResource&& meshResource)
    {
        BenzinAssert(!meshResource.DebugName.empty());
        BenzinAssert(!meshResource.SubMeshes.empty());
        BenzinAssert(!meshResource.SubMeshInstances.empty());
        BenzinAssert(!meshResource.Materials.empty());

        const auto textureOffset = (uint32_t)m_Textures.size();
        PushTextures(meshResource.TextureImages);

        const auto updateTextureIndexIfNeeded = [&](uint32_t& outTextureIndex)
        {
            if (IsValidUnsigned(outTextureIndex))
            {
                outTextureIndex = m_Textures[textureOffset + outTextureIndex]->GetSrv().GetGpuHeapIndex();
            }
        };

        for (auto& material : meshResource.Materials)
        {
            updateTextureIndexIfNeeded(material.AlbedoTextureIndex);
            updateTextureIndexIfNeeded(material.NormalTextureIndex);
            updateTextureIndexIfNeeded(material.MetallicRoughnessTextureIndex);
            updateTextureIndexIfNeeded(material.EmissiveTextureIndex);
        }

        const auto createSubMeshInfos = [](Mesh& outMesh)
        {
            BenzinAssert(outMesh.SubMeshInfos.empty());
            outMesh.SubMeshInfos.reserve(outMesh.SubMeshes.size());

            uint32_t vertexOffset = 0;
            uint32_t indexOffset = 0;

            for (const auto& subMesh : outMesh.SubMeshes)
            {
                outMesh.SubMeshInfos.emplace_back(vertexOffset, indexOffset);

                vertexOffset += (uint32_t)subMesh.Vertices.size();
                indexOffset += (uint32_t)subMesh.Indices.size();
            }

            outMesh.TotalVertexCount = vertexOffset;
            outMesh.TotalIndexCount = indexOffset;
        };

        const entt::entity meshHandle = m_MeshRegistry.create();

        auto& meshDebugName = m_MeshRegistry.emplace<std::string>(meshHandle);
        meshDebugName = std::move(meshResource.DebugName);

        auto& mesh = m_MeshRegistry.emplace<Mesh>(meshHandle);
        mesh.SubMeshes = std::move(meshResource.SubMeshes);
        mesh.SubMeshInstances = std::move(meshResource.SubMeshInstances);
        mesh.Materials = std::move(meshResource.Materials);
        createSubMeshInfos(mesh);

        auto& meshGpuStorage = m_MeshRegistry.emplace<MeshGpuStorage>(meshHandle);
        meshGpuStorage = CreateMeshGpuStorage(m_Device, meshDebugName, mesh);

        UpdateStats(meshHandle);

        return meshHandle;
    }

    void Scene::UploadMeshesToGpu()
    {
        BenzinLogTimeOnScopeExit("Scene::UploadMeshesToGpu");

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
            const auto& meshGpuStorage = m_MeshRegistry.get<MeshGpuStorage>(meshHandle);

            uploadBufferSize += meshGpuStorage.VertexBuffer->GetSize();
            uploadBufferSize += meshGpuStorage.IndexBuffer->GetSize();
            uploadBufferSize += meshGpuStorage.MeshInfoBuffer->GetSize();
        });

        auto& commandList = m_Device.GetGraphicsCommandQueue().GetCommandList(uploadBufferSize);
        m_MeshRegistry.each([this, &commandList](entt::entity meshHandle)
        {
            const auto& mesh = m_MeshRegistry.get<Mesh>(meshHandle);
            const auto& meshGpuStorage = m_MeshRegistry.get<MeshGpuStorage>(meshHandle);

            for (const auto [subMesh, subMeshInfo] : std::views::zip(mesh.SubMeshes, mesh.SubMeshInfos))
            {
                commandList.UploadToBuffer<joint::MeshVertex>(*meshGpuStorage.VertexBuffer, subMesh.Vertices, subMeshInfo.VertexOffset);
                commandList.UploadToBuffer<uint32_t>(*meshGpuStorage.IndexBuffer, subMesh.Indices, subMeshInfo.IndexOffset);
            }

            commandList.UploadToBuffer<joint::MeshInfo>(*meshGpuStorage.MeshInfoBuffer, mesh.SubMeshInfos);
        });
    }

    void Scene::UploadAllMeshInstances()
    {
        Bytes32 uploadBufferSize;
        m_MeshRegistry.each([this, &uploadBufferSize](entt::entity meshHandle)
        {
            const auto& meshGpuStorage = m_MeshRegistry.get<MeshGpuStorage>(meshHandle);

            uploadBufferSize += meshGpuStorage.MeshInstanceBuffer->GetSize();
        });

        auto& commandList = m_Device.GetGraphicsCommandQueue().GetCommandList(uploadBufferSize);
        m_MeshRegistry.each([this, &commandList](entt::entity meshHandle)
        {
            const auto& mesh = m_MeshRegistry.get<Mesh>(meshHandle);
            const auto& meshGpuStorage = m_MeshRegistry.get<MeshGpuStorage>(meshHandle);

            commandList.UploadToBuffer<joint::MeshInstance>(*meshGpuStorage.MeshInstanceBuffer, mesh.SubMeshInstances);
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
        Bytes32 uploadBufferSize;
        m_MeshRegistry.each([this, &uploadBufferSize](entt::entity meshHandle)
        {
            const auto& meshGpuStorage = m_MeshRegistry.get<MeshGpuStorage>(meshHandle);

            uploadBufferSize += meshGpuStorage.MaterialBuffer->GetSize();
        });

        auto& commandList = m_Device.GetGraphicsCommandQueue().GetCommandList(uploadBufferSize);
        m_MeshRegistry.each([this, &commandList](entt::entity meshHandle)
        {
            const auto& mesh = m_MeshRegistry.get<Mesh>(meshHandle);
            const auto& meshGpuStorage = m_MeshRegistry.get<MeshGpuStorage>(meshHandle);

            commandList.UploadToBuffer<joint::Material>(*meshGpuStorage.MaterialBuffer, mesh.Materials);
        });
    }

    void Scene::UpdateStats(entt::entity meshHandle)
    {
        const auto& mesh = m_MeshRegistry.get<Mesh>(meshHandle);

        m_Stats.VertexCount += mesh.TotalVertexCount;
        m_Stats.TriangleCount += mesh.TotalIndexCount / 3;

        m_Stats.MeshCount += (uint32_t)mesh.SubMeshes.size();
        m_Stats.MaterialCount += (uint32_t)mesh.Materials.size();
        m_Stats.MeshInstanceCount += (uint32_t)mesh.SubMeshInstances.size();
    }

}
