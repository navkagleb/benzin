#include "benzin/config/bootstrap.hpp"
#include "benzin/engine/scene.hpp"

#include <shaders/joint/mesh_types.hpp>
#include <shaders/joint/light.hpp>

#include "benzin/core/buffer_writer.hpp"
#include "benzin/core/cmd_line_args.hpp"
#include "benzin/core/engine_math.hpp"
#include "benzin/core/math.hpp"
#include "benzin/core/profiler.hpp"
#include "benzin/core/profiler.hpp"
#include "benzin/core/tick_timer.hpp"
#include "benzin/engine/entity_components.hpp"
#include "benzin/engine/light.hpp"
#include "benzin/engine/mesh.hpp"
#include "benzin/engine/resource_helper.hpp"
#include "benzin/engine/resource_loader.hpp"
#include "benzin/graphics/buffer.hpp"
#include "benzin/graphics/cmd_queue.hpp"
#include "benzin/graphics/device.hpp"
#include "benzin/graphics/texture.hpp"

namespace benzin
{

    Scene::Scene(Device& device)
        : m_Device{ device }
    {
        m_SunEntity = m_EntityRegistry.create();
        m_EntityRegistry.emplace<SunLight>(m_SunEntity);

        MakeUniquePtr(m_LightBuffer, m_Device, BufferCreation
        {
            .DebugName = "LightBuffer",
            .MemoryType = ResourceMemoryType::Upload,
            .Type = BufferType::Structured,
            .ElementSizeInBytes = sizeof(joint::Light),
            .ElementCount = s_MaxLightCount * CmdLineArgs::GetFrameInFlightCount(),
        });
    }

    Scene::~Scene() = default;

    const Descriptor& Scene::GetEntityTransformBufferSrv() const
    {
        BenzinAssert(m_EntityTransformBuffer.get() != nullptr);

        return m_EntityTransformBuffer->GetSrv(SubRange64
        {
            m_EntityTransformCount * m_Device.GetActiveFrameIndex(),
            m_EntityTransformCount,
        });
    }

    const Descriptor& Scene::GetUnifiedMaterialBufferSrv() const
    {
        return m_UnifiedMaterialBuffer->GetSrv();
    }

    uint64_t Scene::GetLightBufferGpuAddress() const
    {
        return m_LightBuffer->GetGpuVirtualAddress(s_MaxLightCount * m_Device.GetActiveFrameIndex());
    }

    entt::entity Scene::AddMesh(MeshResource&& meshResource)
    {
        BenzinAssert(!meshResource.DebugName.empty());
        BenzinAssert(!meshResource.Vertices.empty());
        BenzinAssert(!meshResource.Indices.empty());
        BenzinAssert(!meshResource.DrawRanges.empty());
        BenzinAssert(!meshResource.Instances.empty());

        const uint32_t materialOffset = AddMaterials(meshResource.TextureImages, meshResource.Materials);

        const entt::entity meshHandle = m_MeshRegistry.create();

        auto& meshTag = m_MeshRegistry.emplace<MeshTag>(meshHandle);
        meshTag = std::move(meshResource.DebugName);

        auto& mesh = m_MeshRegistry.emplace<Mesh>(meshHandle);
        mesh.Vertices = std::move(meshResource.Vertices);
        mesh.Indices = std::move(meshResource.Indices);
        mesh.DrawRanges = std::move(meshResource.DrawRanges);
        mesh.Instances = std::move(meshResource.Instances);

        {
            BenzinLogTimeOnScopeExit("{} mesh optimization", meshTag);

            RegroupMesh(mesh);
            OptimizeMesh(mesh);
            GenerateMeshlets(mesh);
            GenerateBoundingSpheres(mesh);
        }

        for (MeshInstance& meshInstance : mesh.Instances)
        {
            meshInstance.MaterialIndex += materialOffset;
        }

        auto& meshGpuStorage = m_MeshRegistry.emplace<MeshGpuStorage>(meshHandle);
        meshGpuStorage = mesh.CreateGpuStorage(m_Device, meshTag);

        return meshHandle;
    }

    void Scene::UploadMeshesToGpu()
    {
        // TODO: CalcUploadBufferSize + UploadToGpu methods to remove reference to GraphicsCmdList in Scene class

        BenzinLogTimeOnScopeExit("Scene::UploadMeshesToGpu");

        const auto view = m_MeshRegistry.view<Mesh, MeshGpuStorage>();

        uint64_t uploadSizeInBytes = 0;
        for (const entt::entity meshHandle : view)
        {
            const auto& meshGpuStorage = view.get<MeshGpuStorage>(meshHandle);

            uploadSizeInBytes += meshGpuStorage.VertexBuffer->GetSizeInBytes();
            uploadSizeInBytes += meshGpuStorage.IndexBuffer->GetSizeInBytes();
            uploadSizeInBytes += meshGpuStorage.ObjectToLocalMatrixBuffer->GetSizeInBytes();
        };

        auto& cmdList = m_Device.GetGraphicsCmdQueue().GetCmdList(uploadSizeInBytes);
        for (const entt::entity meshHandle : view)
        {
            const auto& mesh = view.get<Mesh>(meshHandle);
            const auto& meshGpuStorage = view.get<MeshGpuStorage>(meshHandle);

            cmdList.UploadToBuffer(*meshGpuStorage.VertexBuffer, ToSpan(mesh.Vertices));
            cmdList.UploadToBuffer(*meshGpuStorage.IndexBuffer, ToSpan(mesh.Indices));

            for (const auto& [i, instance] : mesh.Instances | std::views::enumerate)
            {
                cmdList.UploadToBuffer(*meshGpuStorage.ObjectToLocalMatrixBuffer, ToSpan(&instance.ObjectToLocalMatrix), (uint32_t)i);
            }
        }
    }

    void Scene::UploadMeshletsToGpu()
    {
        const auto view = m_MeshRegistry.view<Mesh, MeshGpuStorage>();

        uint64_t uploadSizeInBytes = 0;
        for (const entt::entity meshHandle : view)
        {
            const auto& meshGpuStorage = view.get<MeshGpuStorage>(meshHandle);

            uploadSizeInBytes += meshGpuStorage.MeshletBuffer->GetSizeInBytes();
            uploadSizeInBytes += meshGpuStorage.MeshletIndirectVertexBuffer->GetSizeInBytes();
            uploadSizeInBytes += meshGpuStorage.MeshletIndexBuffer->GetSizeInBytes();
        };

        auto& cmdList = m_Device.GetGraphicsCmdQueue().GetCmdList(uploadSizeInBytes);
        for (const entt::entity meshHandle : view)
        {
            const auto& mesh = view.get<Mesh>(meshHandle);
            const auto& meshGpuStorage = view.get<MeshGpuStorage>(meshHandle);

            cmdList.UploadToBuffer(*meshGpuStorage.MeshletBuffer, ToSpan(mesh.Meshlets));
            cmdList.UploadToBuffer(*meshGpuStorage.MeshletIndirectVertexBuffer, ToSpan(mesh.MeshletIndirectVertices));
            cmdList.UploadToBuffer(*meshGpuStorage.MeshletIndexBuffer, ToSpan(mesh.MeshletIndices));
        }
    }

    void Scene::UploadMaterialsToGpu()
    {
        BenzinLogTimeOnScopeExit("Scene::UploadMaterialsToGpu");

        UploadPixelDataSetToGpu();

        MakeUniquePtr(m_UnifiedMaterialBuffer, m_Device, BufferCreation
        {
            .DebugName = "Scene_UnifiedMaterialBuffer",
            .Type = BufferType::Structured,
            .ElementSizeInBytes = sizeof(joint::Material),
            .ElementCount = (uint32_t)m_UnifiedMaterials.size(),
        });

        auto& cmdList = m_Device.GetGraphicsCmdQueue().GetCmdList(m_UnifiedMaterialBuffer->GetSizeInBytes());
        for (uint32_t i = 0; i < m_UnifiedMaterials.size(); ++i)
        {
            const Material& material = m_UnifiedMaterials[i];

            const joint::Material gpuMaterial
            {
                .AlbedoTextureHeapIndex = material.TextureGpuHeapIndices.Albedo,
                .NormalTextureHeapIndex = material.TextureGpuHeapIndices.Normal,
                .MetallicRoughnessTextureHeapIndex = material.TextureGpuHeapIndices.MetallicRoughness,
                .EmissiveTextureHeapIndex = material.TextureGpuHeapIndices.Emissive,
                .AlbedoFactor = material.Consts.AlbedoFactor,
                .AlphaCutoff = material.Consts.AlphaCutoff,
                .NormalScale = material.Consts.NormalScale,
                .MetalnessFactor = material.Consts.MetalnessFactor,
                .RoughnessFactor = material.Consts.RoughnessFactor,
                .OcclusionStrenght = material.Consts.OcclusionStrenght,
                .EmissiveFactor = material.Consts.EmissiveFactor,
            };

            cmdList.UploadToBuffer(*m_UnifiedMaterialBuffer, ToSpan(&gpuMaterial), i);
        }
    }

    uint32_t Scene::AddTextures(std::span<TextureImage> textureImages)
    {
        const auto textureOffset = (uint32_t)m_Textures.size();

        m_PixelDataSet.reserve(textureOffset + textureImages.size());
        m_Textures.reserve(textureOffset + textureImages.size());

        for (TextureImage& textureImage : textureImages)
        {
            m_PixelDataSet.push_back(std::move(textureImage.PixelData));

            m_Textures.push_back(std::make_unique<Texture>(m_Device, TextureCreation
            {
                .DebugName = textureImage.DebugName,
                .Format = textureImage.Format,
                .Width = textureImage.Width,
                .Height = textureImage.Height,
                .MipCount = 1, // TODO: Mip generation
            }));
        }

        return textureOffset;
    }

    uint32_t Scene::AddMaterials(std::span<TextureImage> textureImages, std::span<const MeshResource::Material> materials)
    {
        BenzinAssert(!materials.empty());

        const uint32_t textureOffset = AddTextures(textureImages);

        const auto materialOffset = (uint32_t)m_UnifiedMaterials.size();
        m_UnifiedMaterials.reserve(materialOffset + materials.size());

        for (const MeshResource::Material& materialResource : materials)
        {
            Material& material = m_UnifiedMaterials.emplace_back();
            material.Consts = materialResource.Consts;
            material.TextureGpuHeapIndices.Albedo = GetTextureGpuHeapIndex(textureOffset, materialResource.TextureIndices.Albedo);
            material.TextureGpuHeapIndices.Normal = GetTextureGpuHeapIndex(textureOffset, materialResource.TextureIndices.Normal);
            material.TextureGpuHeapIndices.MetallicRoughness = GetTextureGpuHeapIndex(textureOffset, materialResource.TextureIndices.MetallicRoughness);
            material.TextureGpuHeapIndices.Emissive = GetTextureGpuHeapIndex(textureOffset, materialResource.TextureIndices.Emissive);
        }

        return materialOffset;
    }

    uint32_t Scene::GetTextureGpuHeapIndex(uint32_t textureOffset, uint32_t localTextureIndex) const
    {
        if (!IsGoodUint(localTextureIndex))
        {
            return g_Bad32;
        }

        BenzinAssert(textureOffset + localTextureIndex < m_Textures.size());
        return m_Textures[textureOffset + localTextureIndex]->GetSrv().GetGpuHeapIndex();
    }

    void Scene::UploadPixelDataSetToGpu()
    {
        if (m_Textures.empty())
        {
            return;
        }

        uint64_t uploadSizeInBytes = 0;
        for (const auto& texture : m_Textures)
        {
            uploadSizeInBytes += AlignUp(texture->GetSizeInBytes(), GraphicsConfig::GetTextureAlignmentInBytes());
        }

        auto& cmdList = m_Device.GetGraphicsCmdQueue().GetCmdList(uploadSizeInBytes);
        for (const auto& [pixelData, texture] : std::views::zip(m_PixelDataSet, m_Textures))
        {
            cmdList.UploadToTexture(*texture, pixelData);
        }

        m_PixelDataSet.clear();
    }

    void Scene::UpdateEntities()
    {
        const auto view = m_EntityRegistry.view<EntityUpdateCallback>();
        for (const entt::entity entityHandle : view)
        {
            const auto& callback = view.get<EntityUpdateCallback>(entityHandle);

            BenzinAssert((bool)callback);
            callback();
        }
    }

    void Scene::UploadEntityTransformsToGpu()
    {
        const uint32_t frameInFlightCount = CmdLineArgs::GetFrameInFlightCount();

        const auto meshView = m_EntityRegistry.view<MeshInstanceComponent, Transform>();
        const auto lightView = m_EntityRegistry.view<MeshInstanceComponent, SphericalLight>();

        m_EntityTransformCount = (uint32_t)(meshView.size_hint() + lightView.size_hint()); // TODO: Light::IsEnabled
        if (m_EntityTransformBuffer.get() == nullptr || m_EntityTransformBuffer->GetElementCount() != m_EntityTransformCount * frameInFlightCount)
        {
            MakeUniquePtr(m_EntityTransformBuffer, m_Device, BufferCreation
            {
                .DebugName = "Scene_EntityTransformBuffer",
                .MemoryType = ResourceMemoryType::Upload, // TODO
                .Type = BufferType::Structured,
                .ElementSizeInBytes = sizeof(joint::EntityTransform),
                .ElementCount = m_EntityTransformCount * frameInFlightCount,
            });
        }

        BufferWriter entityTransformWriter{ m_EntityTransformBuffer->GetCpuMappedData(), m_EntityTransformBuffer->GetSizeInBytes() };
        entityTransformWriter.SetElementPosition<joint::EntityTransform>(m_EntityTransformCount * m_Device.GetActiveFrameIndex());

        uint32_t entityTransformIndex = 0;

        for (const auto entity : meshView)
        {
            auto& meshInstanceComponent = meshView.get<MeshInstanceComponent>(entity);
            meshInstanceComponent.m_EntityTransformIndex = entityTransformIndex++;

            const auto& transform = meshView.get<Transform>(entity);
            entityTransformWriter.WriteRaw(joint::EntityTransform
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

            auto& meshInstanceComponent = meshView.get<MeshInstanceComponent>(entity);
            meshInstanceComponent.m_EntityTransformIndex = entityTransformIndex++;

            entityTransformWriter.WriteRaw(joint::EntityTransform
            {
                .LocalToWorld = light.GetTransform().GetLocalToWorldMatrix(),
                .PrevLocalToWorld = light.GetTransform().GetPrevLocalToWorldMatrix(),
            });
        }
    }

    void Scene::UploadLightsToGpu()
    {
        m_ActiveLightCount = 1;

        BufferWriter lights{ m_LightBuffer->GetCpuMappedData(), m_LightBuffer->GetSizeInBytes() };
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
