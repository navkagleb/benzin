#include "benzin/config/bootstrap.hpp"
#include "benzin/engine/scene.hpp"

#include <shaders/joint/mesh_types.hpp>
#include <shaders/joint/light.hpp>

#include "benzin/core/buffer_writer.hpp"
#include "benzin/core/command_line_args.hpp"
#include "benzin/core/engine_math.hpp"
#include "benzin/core/profiler.hpp"
#include "benzin/core/math.hpp"
#include "benzin/core/profiler.hpp"
#include "benzin/core/tick_timer.hpp"
#include "benzin/engine/entity_components.hpp"
#include "benzin/engine/light.hpp"
#include "benzin/engine/mesh.hpp"
#include "benzin/engine/resource_loader.hpp"
#include "benzin/graphics/buffer.hpp"
#include "benzin/graphics/command_queue.hpp"
#include "benzin/graphics/device.hpp"
#include "benzin/graphics/texture.hpp"

namespace benzin
{

    static MeshGpuStorage CreateMeshGpuStorage(Device& device, std::string_view debugName, const Mesh& mesh)
    {
        MeshGpuStorage meshGpuStorage;

        MakeUniquePtr(meshGpuStorage.VertexBuffer, device, BufferCreation
        {
            .DebugName = std::format("{}_VertexBuffer", debugName),
            .Type = BufferType::Vertex,
            .ElementSize = sizeof(joint::MeshVertex),
            .ElementCount = (uint32_t)mesh.TotalVertexCount,
        });

        MakeUniquePtr(meshGpuStorage.IndexBuffer, device, BufferCreation
        {
            .DebugName = std::format("{}_IndexBuffer", debugName),
            .Type = BufferType::Index,
            .Format = GraphicsFormat::R32Uint,
            .ElementSize = sizeof(uint32_t),
            .ElementCount = (uint32_t)mesh.TotalIndexCount,
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

    Scene::Scene(Device& device, TickTimer& animationTimer)
        : m_Device{ device }
        , m_AnimationTimer{ animationTimer }
    {
        m_SunEntity = m_EntityRegistry.create();
        m_EntityRegistry.emplace<SunLight>(m_SunEntity);

        const uint32_t frameInFlightCount = CommandLineArgs::GetU32("FrameInFlightCount");

        MakeUniquePtr(m_LightBuffer, m_Device, BufferCreation
        {
            .DebugName = "LightBuffer",
            .MemoryType = ResourceMemoryType::Upload,
            .Type = BufferType::Structured,
            .ElementSize = sizeof(joint::Light),
            .ElementCount = s_MaxLightCount * frameInFlightCount,
        });
    }

    Scene::~Scene() = default;

    Descriptor Scene::GetTransformBufferSrv() const
    {
        BenzinAssert(m_TransformBuffer.get() != nullptr);

        return m_TransformBuffer->GetSrv(IndexRange32
        {
            m_TransformCount * m_Device.GetActiveFrameIndex(),
            m_TransformCount,
        });
    }

    uint64_t Scene::GetLightBufferGpuAddress() const
    {
        return m_LightBuffer->GetGpuVirtualAddress(s_MaxLightCount * m_Device.GetActiveFrameIndex());
    }

    void Scene::OnUpdate()
    {
        BenzinProfile();

        UpdateEntities();

        UploadTransformsToGpu();
        UploadLightsToGpu();
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
        mesh.IsIndexOrderClockwise = meshResource.IsIndexOrderClockwise;

        createSubMeshInfos(mesh);

        auto& meshGpuStorage = m_MeshRegistry.emplace<MeshGpuStorage>(meshHandle);
        meshGpuStorage = CreateMeshGpuStorage(m_Device, meshDebugName, mesh);

        UpdateStats(meshHandle);

        return meshHandle;
    }

    void Scene::UploadMeshesToGpu()
    {
        // TODO: CalcUploadBufferSize + UploadToGpu methods to remove reference to GraphicsCommandList in Scene class

        BenzinLogTimeOnScopeExit("Scene::UploadMeshesToGpu");

        UploadAllMeshData();
        UploadAllMeshInstances();
        UploadAllTextures();
        UploadAllMaterials();
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

            for (const auto& [i, material] : mesh.Materials | std::views::enumerate)
            {
                // TODO: Potentially very tricky place
                // Cut the last member of benzin::Material
                const auto data = ToSingleByteSpan(material, sizeof(joint::Material)); 
                const size_t offsetInBytes = i * data.size_bytes();
                commandList.UploadToBuffer(*meshGpuStorage.MaterialBuffer, data, offsetInBytes);
            }
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

    void Scene::UpdateEntities()
    {
        if (m_AnimationTimer.IsPaused())
        {
            return;
        }

        const auto view = m_EntityRegistry.view<EntityUpdateCallback>();
        for (const auto& [_, callback] : view.each())
        {
            BenzinAssert((bool)callback);
            callback();
        }
    }

    void Scene::UploadTransformsToGpu()
    {
        const uint32_t frameInFlightCount = CommandLineArgs::GetU32("FrameInFlightCount");

        const auto meshView = m_EntityRegistry.view<MeshComponent, Transform>();
        const auto lightView = m_EntityRegistry.view<MeshComponent, SphericalLight>();

        m_TransformCount = (uint32_t)(meshView.size_hint() + lightView.size_hint()); // TODO: Light::IsEnabled
        if (m_TransformBuffer.get() == nullptr || m_TransformBuffer->GetElementCount() != m_TransformCount * frameInFlightCount)
        {
            MakeUniquePtr(m_TransformBuffer, m_Device, BufferCreation
            {
                .DebugName = "TransformBuffer",
                .MemoryType = ResourceMemoryType::Upload, // TODO
                .Type = BufferType::Structured,
                .ElementSize = sizeof(joint::MeshTransform),
                .ElementCount = m_TransformCount * frameInFlightCount,
            });
        }

        BufferWriter transformWriter{ m_TransformBuffer->GetCpuMappedData(), m_TransformBuffer->GetSize() };
        transformWriter.SetElementPosition<joint::MeshTransform>(m_TransformCount * m_Device.GetActiveFrameIndex());

        uint32_t gpuTransformIndex = 0;

        for (const auto entity : meshView)
        {
            auto& meshCompoonent = meshView.get<MeshComponent>(entity);
            meshCompoonent.GpuTransformIndex = gpuTransformIndex++;

            const auto& transform = meshView.get<Transform>(entity);
            transformWriter.WriteRaw(joint::MeshTransform
            {
                .LocalToWorld = transform.GetLocalToWorldMatrix(),
                .PrevLocalToWorld = transform.GetPrevLocalToWorldMatrix(),
            });
        }

        for (const auto entity : lightView)
        {
            const auto& light = lightView.get<SphericalLight>(entity);

            if (!light.IsEnabled())
            {
                continue;
            }

            auto& meshCompoonent = meshView.get<MeshComponent>(entity);
            meshCompoonent.GpuTransformIndex = gpuTransformIndex++;

            transformWriter.WriteRaw(joint::MeshTransform
            {
                .LocalToWorld = light.GetTransform().GetLocalToWorldMatrix(),
                .PrevLocalToWorld = light.GetTransform().GetPrevLocalToWorldMatrix(),
            });
        }
    }

    void Scene::UploadLightsToGpu()
    {
        m_ActiveLightCount = 1;

        BufferWriter lights{ m_LightBuffer->GetCpuMappedData(), m_LightBuffer->GetSize() };
        lights.SetElementPosition<joint::Light>(s_MaxLightCount * m_Device.GetActiveFrameIndex());

        {
            const auto& sunLight = m_EntityRegistry.get<SunLight>(m_SunEntity);

            lights.WriteRaw(joint::Light
            {
                .Color = sunLight.GetColor(),
                .Intensity = sunLight.GetIntensity(),
                .WorldPosition = sunLight.CalcToSunDirection(),
                .WorldRadius = std::tan(sunLight.GetAngularDiameterInRadians() * 0.5f),
                .Attenuation = {},
                .Type = joint::LightType::Sun,
            });
        }

        const auto view = m_EntityRegistry.view<SphericalLight>();
        for (const auto [entityHandle, light] : view.each())
        {
            if (!light.IsEnabled())
            {
                continue;
            }

            lights.WriteRaw(joint::Light
            {
                .Color = light.GetColor(),
                .Intensity = light.GetIntensity(),
                .WorldPosition = light.GetPosition(),
                .WorldRadius = light.GetRadius(),
                .Attenuation = light.GetAttenuation(),
                .Type = joint::LightType::Spherical,
            });

            m_ActiveLightCount++;
        }
    }

}
