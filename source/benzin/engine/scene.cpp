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
#include "benzin/engine/geometry_generator.hpp"
#include "benzin/engine/light.hpp"
#include "benzin/engine/mesh.hpp"
#include "benzin/engine/resource_helper.hpp"
#include "benzin/engine/resource_loader.hpp"
#include "benzin/graphics/buffer.hpp"
#include "benzin/graphics/cmd_queue.hpp"
#include "benzin/graphics/device.hpp"
#include "benzin/graphics/gpu_heap.hpp"
#include "benzin/graphics/texture.hpp"

namespace benzin
{

    static MeshResource CreateUnitSphereMesh()
    {
        benzin::MeshData meshData = benzin::GetUnitGeoSphereMesh();

        benzin::MeshDrawRange drawRange;
        drawRange.m_VertexRange = ToSpan(meshData.Vertices);
        drawRange.m_IndexRange = ToSpan(meshData.Indices);
        drawRange.m_Topology = meshData.PrimitiveTopology;

        benzin::MeshInstance instance;
        instance.m_DrawRangeIndex = 0;

        benzin::MeshResource meshResource;
        meshResource.m_Vertices = std::move(meshData.Vertices);
        meshResource.m_Indices = std::move(meshData.Indices);
        meshResource.m_DrawRanges.push_back(drawRange);
        meshResource.m_Instances.push_back(instance);

        return meshResource;
    };

    //

    Scene::Scene(Device& device)
        : m_Device{ device }
    {
        m_SunEntity = m_EntityRegistry.create();
        m_EntityRegistry.emplace<SunLight>(m_SunEntity);

        m_UnitSphereMeshHandle = AddMesh("UnitSphere", CreateUnitSphereMesh());

        Material fallbackMaterial;
        fallbackMaterial.Consts.m_AlbedoFactor = { 1.0f, 0.0f, 1.0f, 1.0f };
        m_UnifiedMaterials.push_back(fallbackMaterial);
    }

    Scene::~Scene() = default;

    const Material& Scene::GetMaterial(uint32_t index) const
    {
        BenzinAssert(index < m_UnifiedMaterials.size());
        return m_UnifiedMaterials[index];
    }

    entt::entity Scene::AddMesh(
        std::string_view debugName,
        MeshResource&& meshResource,
        std::vector<MaterialResource>&& materials,
        std::vector<TextureImage>&& textures)
    {
        const uint32_t materialOffset = AddMaterials(textures, materials);

        const entt::entity meshHandle = m_MeshRegistry.create();

        auto& meshTag = m_MeshRegistry.emplace<MeshTag>(meshHandle);
        meshTag = debugName;

        auto& mesh = m_MeshRegistry.emplace<Mesh>(meshHandle);
        mesh.m_Vertices = std::move(meshResource.m_Vertices);
        mesh.m_Indices = std::move(meshResource.m_Indices);
        mesh.m_DrawRanges = std::move(meshResource.m_DrawRanges);
        mesh.m_Instances = std::move(meshResource.m_Instances);

        {
            BenzinLogTimeOnScopeExit("{} mesh optimization + meshlet generation", meshTag);

            OptimizeMesh(mesh);
            GenerateMeshlets(mesh);
            GenerateBoundingSpheres(mesh);
        }

        for (MeshInstance& meshInstance : mesh.m_Instances)
        {
            if (IsGoodUint(meshInstance.m_MaterialIndex))
            {
                meshInstance.m_MaterialIndex += materialOffset;
            }
            else
            {
                meshInstance.m_MaterialIndex = 0; // Fallback material index
            }
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
        };

        auto& cmdList = m_Device.GetGraphicsCmdQueue().GetCmdList(uploadSizeInBytes);
        for (const entt::entity meshHandle : view)
        {
            const auto& mesh = view.get<Mesh>(meshHandle);
            const auto& meshGpuStorage = view.get<MeshGpuStorage>(meshHandle);

            cmdList.UploadToBuffer(*meshGpuStorage.VertexBuffer, ToSpan(mesh.m_Vertices));
            cmdList.UploadToBuffer(*meshGpuStorage.IndexBuffer, ToSpan(mesh.m_Indices));
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
            uploadSizeInBytes += meshGpuStorage.MeshletCullVolumeBuffer->GetSizeInBytes();
            uploadSizeInBytes += meshGpuStorage.MeshletIndirectVertexBuffer->GetSizeInBytes();
            uploadSizeInBytes += meshGpuStorage.MeshletIndexBuffer->GetSizeInBytes();
        };

        auto& cmdList = m_Device.GetGraphicsCmdQueue().GetCmdList(uploadSizeInBytes);
        for (const entt::entity meshHandle : view)
        {
            const auto& mesh = view.get<Mesh>(meshHandle);
            const auto& meshGpuStorage = view.get<MeshGpuStorage>(meshHandle);

            cmdList.UploadToBuffer(*meshGpuStorage.MeshletBuffer, ToSpan(mesh.m_Meshlets));
            cmdList.UploadToBuffer(*meshGpuStorage.MeshletCullVolumeBuffer, ToSpan(mesh.m_MeshletCullVolumes));
            cmdList.UploadToBuffer(*meshGpuStorage.MeshletIndirectVertexBuffer, ToSpan(mesh.m_MeshletIndirectVertices));
            cmdList.UploadToBuffer(*meshGpuStorage.MeshletIndexBuffer, ToSpan(mesh.m_MeshletIndices));
        }
    }

    void Scene::UploadMaterialsToGpu()
    {
        BenzinLogTimeOnScopeExit("Scene::UploadMaterialsToGpu");

        UploadPixelDataSetToGpu();

        std::vector<joint::Material> unifiedMaterials;
        unifiedMaterials.reserve(m_UnifiedMaterials.size());

        for (const Material& material : m_UnifiedMaterials)
        {
            unifiedMaterials.push_back(joint::Material
            {
                .AlbedoTextureHeapIndex = material.TextureGpuHeapIndices.m_Albedo,
                .NormalTextureHeapIndex = material.TextureGpuHeapIndices.m_Normal,
                .MetallicRoughnessTextureHeapIndex = material.TextureGpuHeapIndices.m_MetallicRoughness,
                .EmissiveTextureHeapIndex = material.TextureGpuHeapIndices.m_Emissive,
                .AlbedoFactor = material.Consts.m_AlbedoFactor,
                .AlphaCutoff = material.Consts.m_AlphaCutoff,
                .NormalScale = material.Consts.m_NormalScale,
                .MetalnessFactor = material.Consts.m_MetalnessFactor,
                .RoughnessFactor = material.Consts.m_RoughnessFactor,
                .OcclusionStrenght = material.Consts.m_OcclusionStrenght,
                .EmissiveFactor = material.Consts.m_EmissiveFactor,
            });
        }

        m_UnifiedMaterialBuffer = m_Device.GetPersistentDefaultLinearAllocator().AllocateBuffer("Scene_UnifiedMaterialBuffer", ToSpan(unifiedMaterials));
        
        auto& cmdList = m_Device.GetGraphicsCmdQueue().GetCmdList(m_UnifiedMaterialBuffer->GetSizeInBytes());
        cmdList.UploadToBuffer(*m_UnifiedMaterialBuffer, ToSpan(unifiedMaterials));
    }

    uint32_t Scene::AddTextures(std::span<TextureImage> textureImages)
    {
        const auto textureOffset = (uint32_t)m_Textures.size();

        m_PixelDataSet.reserve(textureOffset + textureImages.size());
        m_Textures.reserve(textureOffset + textureImages.size());

        for (TextureImage& textureImage : textureImages)
        {
            m_PixelDataSet.push_back(std::move(textureImage.m_PixelData));

            TextureCreation creation;
            creation.DebugName = textureImage.m_DebugName;
            creation.Format = textureImage.m_Format;
            creation.Width = textureImage.m_Width;
            creation.Height = textureImage.m_Height;
            creation.MipCount = 1; // TODO: Mip generation
            m_Textures.push_back(std::make_unique<Texture>(m_Device, creation));
        }

        return textureOffset;
    }

    uint32_t Scene::AddMaterials(std::span<TextureImage> textureImages, std::span<const MaterialResource> materials)
    {
        const auto materialOffset = (uint32_t)m_UnifiedMaterials.size();

        if (materials.empty())
            return materialOffset;

        const uint32_t textureOffset = AddTextures(textureImages);

        m_UnifiedMaterials.reserve(materialOffset + materials.size());

        for (const MaterialResource& materialResource : materials)
        {
            Material& material = m_UnifiedMaterials.emplace_back();
            material.Consts = materialResource.m_Consts;
            material.TextureGpuHeapIndices.m_Albedo = GetTextureGpuHeapIndex(textureOffset, materialResource.m_TextureIndices.m_Albedo);
            material.TextureGpuHeapIndices.m_Normal = GetTextureGpuHeapIndex(textureOffset, materialResource.m_TextureIndices.m_Normal);
            material.TextureGpuHeapIndices.m_MetallicRoughness = GetTextureGpuHeapIndex(textureOffset, materialResource.m_TextureIndices.m_MetallicRoughness);
            material.TextureGpuHeapIndices.m_Emissive = GetTextureGpuHeapIndex(textureOffset, materialResource.m_TextureIndices.m_Emissive);
        }

        return materialOffset;
    }

    uint32_t Scene::GetTextureGpuHeapIndex(uint32_t textureOffset, uint32_t localTextureIndex) const
    {
        if (!IsGoodUint(localTextureIndex))
            return g_Bad32;

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
            uploadSizeInBytes += AlignUp(texture->GetSizeInBytes(), D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT);
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
        BenzinProfile();

        const auto view = m_EntityRegistry.view<EntityUpdateCallback>();
        for (const entt::entity entityHandle : view)
        {
            const auto& callback = view.get<EntityUpdateCallback>(entityHandle);

            BenzinAssert((bool)callback);
            callback();
        }
    }

    void Scene::UploadLightsToGpu()
    {
        BenzinProfile();

        std::vector<joint::Light> activeLights;

        {
            const auto& sunLight = m_EntityRegistry.get<SunLight>(m_SunEntity);

            activeLights.push_back(joint::Light
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
        for (const entt::entity entityHandle : view)
        {
            const auto& light = view.get<SphericalLight>(entityHandle);

            if (!light.IsEnabled())
            {
                continue;
            }

            activeLights.push_back(joint::Light
            {
                .Color = light.GetColor(),
                .Intensity = light.GetIntensity(),
                .WorldPosition = light.GetPosition(),
                .WorldRadius = light.GetRadius(),
                .Attenuation = light.GetAttenuation(),
                .Type = joint::LightType::Spherical,
            });
        }

        m_LightBuffer = m_Device.GetTemporalLinearAllocator().AllocateAndWriteBuffer("Scene_Lights", ToSpan(activeLights));
        m_ActiveLightCount = (uint32_t)activeLights.size();
    }

    void Scene::EndFrame()
    {
        // D3D12 WARNING : ID3D12CommandList::SetComputeRootShaderResourceView : 1 resources contain
        // the GPU Virtual Address range[0x0000000300440000, 0x0000000300440000] on a Heap(0x00000176BAD3D340:'TemporalHeap_2').
        // This may be OK as long as only one of these resources is actively being used.However, there is no definitive way
        // for the debug layer to identify which resource is intended.Consider using AssertResourceState to help with state validation.
        // [STATE_CREATION WARNING #926: HEAP_ADDRESS_RANGE_INTERSECTS_MULTIPLE_BUFFERS]
        // D3D12 : **BREAK** enabled for the previous message, which was : [WARNING STATE_CREATION #926: HEAP_ADDRESS_RANGE_INTERSECTS_MULTIPLE_BUFFERS]
        m_LightBuffer.reset();
    }

}
