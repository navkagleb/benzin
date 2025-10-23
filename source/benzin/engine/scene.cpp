#include <benzin/config/bootstrap.hpp>
#include <benzin/engine/scene.hpp>

#include <benzin/core/cmd_line_args.hpp>
#include <benzin/core/math.hpp>
#include <benzin/core/math.hpp>
#include <benzin/core/profiler.hpp>
#include <benzin/engine/geometry_generator.hpp>
#include <benzin/engine/mesh.hpp>
#include <benzin/engine/resource_helper.hpp>
#include <benzin/engine/resource_loader.hpp>
#include <benzin/graphics/buffer.hpp>
#include <benzin/graphics/cmd_queue.hpp>
#include <benzin/graphics/device.hpp>
#include <benzin/graphics/gpu_heap.hpp>
#include <benzin/graphics/texture.hpp>

#include <shaders/joint/mesh_types.hpp>

namespace benzin
{

    static void CreateUnitSphereMesh(Mesh& mesh, std::vector<MeshDrawPart>& meshDrawParts)
    {
        MeshData meshData = GetUnitGeoSphereMesh();

        MeshPart part;
        part.m_VertexCount = (uint32_t)meshData.Vertices.size();
        part.m_IndexCount = (uint32_t)meshData.Indices.size();
        part.m_D3D12PrimitiveTopology = meshData.D3D12PrimitiveTopology;

        mesh.m_Vertices = std::move(meshData.Vertices);
        mesh.m_Indices = std::move(meshData.Indices);
        mesh.m_Parts.push_back(part);

        MeshDrawPart drawPart;
        drawPart.m_PartIndex = 0;
        meshDrawParts.push_back(drawPart);
    };

    //

    Scene::Scene(Device& device)
        : m_Device{ device }
    {
        Mesh sphereMesh;
        std::vector<MeshDrawPart> sphereDrawParts;
        CreateUnitSphereMesh(sphereMesh, sphereDrawParts);
        AddMesh("UnitSphere", std::move(sphereMesh), std::move(sphereDrawParts));

        m_Materials.push_back(Material
        {
            .m_AlbedoFactor = { 1.0f, 0.0f, 1.0f, 1.0f },
        });
    }

    Scene::~Scene() = default;

    void Scene::AddMesh(
        const std::string& debugName,
        Mesh&& mesh,
        std::vector<MeshDrawPart>&& meshDrawParts,
        std::vector<Material>&& materials,
        std::vector<TextureImage>&& textures)
    {
        {
            BenzinTraceScopeTime("{} mesh optimization + meshlet generation", debugName);

            OptimizeMesh(mesh);
            GenerateMeshlets(mesh);
            GenerateBoundingSpheres(mesh);
        }

        const auto textureOffset = (uint32_t)m_Textures.size();

        m_Textures.reserve(textureOffset + textures.size());
        m_TexturesData.reserve(textureOffset + textures.size());

        for (TextureImage& textureImage : textures)
        {
            TextureCreation creation;
            creation.DebugName = textureImage.m_DebugName;
            creation.Format = textureImage.m_Format;
            creation.Width = textureImage.m_Width;
            creation.Height = textureImage.m_Height;
            creation.MipCount = 1; // TODO: Mip generation

            m_Textures.push_back(std::make_unique<Texture>(m_Device, creation));
            m_TexturesData.push_back(std::move(textureImage.m_PixelData));
        }

        const auto updateTextureIndex = [this, textureOffset](uint32_t& textureIndex)
        {
            if (textureIndex == g_MaxU32)
                return;

            textureIndex = m_Textures[textureIndex + textureOffset]->GetSrv().GetGpuHeapIndex();
        };

        for (Material& material : materials)
        {
            updateTextureIndex(material.m_AlbedoTextureIndex);
            updateTextureIndex(material.m_NormalTextureIndex);
            updateTextureIndex(material.m_MetallicRoughnessTextureIndex);
            updateTextureIndex(material.m_EmissiveTextureIndex);
        }

        for (MeshPart& part : mesh.m_Parts)
        {
            part.m_VertexOffset += (uint32_t)m_Vertices.size();
            part.m_IndexOffset += (uint32_t)m_Indices.size();
        }

        for (MeshDrawPart& meshDrawPart : meshDrawParts)
        {
            meshDrawPart.m_PartIndex += (uint32_t)m_MeshParts.size();
            
            if (meshDrawPart.m_MaterialIndex == g_MaxU32)
            {
                meshDrawPart.m_MaterialIndex = 0; // Fallback material index
            }
            else
            {
                meshDrawPart.m_MaterialIndex += (uint32_t)m_Materials.size();
            }
        }

        m_MeshRangeMap[debugName] = (uint32_t)m_MeshRanges.size();
        m_MeshRanges.push_back(MeshRange
        {
            .m_DrawPartOffset = (uint32_t)m_MeshDrawParts.size(),
            .m_DrawPartCount = (uint32_t)meshDrawParts.size(),
        });

        m_Vertices.append_range(std::move(mesh.m_Vertices));
        m_Indices.append_range(std::move(mesh.m_Indices));
        m_MeshParts.append_range(std::move(mesh.m_Parts));
        m_MeshDrawParts.append_range(std::move(meshDrawParts));
        m_Materials.append_range(std::move(materials));
    }

    void Scene::UploadToGpu()
    {
        // TODO: CalcUploadBufferSize + UploadToGpu methods to remove reference to GraphicsCmdList in Scene class

        BenzinTraceScopeTime("Scene::UploadToGpu");

        {
            uint64_t uploadSizeInBytes = 0;
            for (const auto& texture : m_Textures)
            {
                uploadSizeInBytes += AlignUp(texture->GetSizeInBytes(), D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT);
            }

            CopyCmdList& cmdList = m_Device.GetGraphicsCmdQueue().GetCmdList(uploadSizeInBytes);
            for (uint32_t i = 0; i < m_Textures.size(); ++i)
            {
                cmdList.UploadToTexture(*m_Textures[i], m_TexturesData[i]);
            }
        }

        std::vector<joint::Material> jointMaterials;
        jointMaterials.reserve(m_Materials.size());

        for (const Material& material : m_Materials)
        {
            joint::Material& jointMaterial = jointMaterials.emplace_back();
            jointMaterial.m_AlbedoTextureHeapIndex = material.m_AlbedoTextureIndex;
            jointMaterial.m_NormalTextureHeapIndex = material.m_NormalTextureIndex;
            jointMaterial.m_MetallicRoughnessTextureHeapIndex = material.m_MetallicRoughnessTextureIndex;
            jointMaterial.m_EmissiveTextureHeapIndex = material.m_EmissiveTextureIndex;
            jointMaterial.m_AlbedoFactor = material.m_AlbedoFactor;
            jointMaterial.m_AlphaCutoff = material.m_AlphaCutoff;
            jointMaterial.m_NormalScale = material.m_NormalScale;
            jointMaterial.m_MetalnessFactor = material.m_MetalnessFactor;
            jointMaterial.m_RoughnessFactor = material.m_RoughnessFactor;
            jointMaterial.m_EmissiveFactor = material.m_EmissiveFactor;
        }

        // TODO: Duplication
        std::vector<joint::MeshDrawPart> jointMeshDrawParts;
        jointMeshDrawParts.reserve(m_MeshDrawParts.size());

        for (const MeshDrawPart& meshDrawPart : m_MeshDrawParts)
        {
            joint::MeshDrawPart& jointMeshDrawPart = jointMeshDrawParts.emplace_back();
            jointMeshDrawPart.m_ObjectToLocal = meshDrawPart.m_ObjectToLocal;
            jointMeshDrawPart.m_PartIndex = meshDrawPart.m_PartIndex;
            jointMeshDrawPart.m_MaterialIndex = meshDrawPart.m_MaterialIndex;
        }

        m_VertexBuffer = m_Device.GetPersistentDefaultLinearAllocator().AllocateBuffer("Scene::VertexBuffer", ToSpan(m_Vertices));
        m_IndexBuffer = m_Device.GetPersistentDefaultLinearAllocator().AllocateBuffer("Scene::IndexBuffer", ToSpan(m_Indices), GraphicsFormat::R32Uint);
        m_MeshDrawPartBuffer = m_Device.GetPersistentDefaultLinearAllocator().AllocateBuffer("Scene::MeshDrawPartBuffer", ToSpan(jointMeshDrawParts));
        m_MaterialBuffer = m_Device.GetPersistentDefaultLinearAllocator().AllocateBuffer("Scene::MaterialBuffer", ToSpan(jointMaterials));

        const uint64_t uploadSizeInBytes =
            m_VertexBuffer->GetSizeInBytes() +
            m_IndexBuffer->GetSizeInBytes() +
            m_MeshDrawPartBuffer->GetSizeInBytes() +
            m_MaterialBuffer->GetSizeInBytes();

        CopyCmdList& cmdList = m_Device.GetGraphicsCmdQueue().GetCmdList(uploadSizeInBytes);
        cmdList.UploadToBuffer(*m_VertexBuffer, ToSpan(m_Vertices));
        cmdList.UploadToBuffer(*m_IndexBuffer, ToSpan(m_Indices));
        cmdList.UploadToBuffer(*m_MeshDrawPartBuffer, ToSpan(jointMeshDrawParts));
        cmdList.UploadToBuffer(*m_MaterialBuffer, ToSpan(jointMaterials));
    }

    void Scene::UploadMeshletsToGpu()
    {
#if 0
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
#endif
    }

    void Scene::ExecuteUpdateCallbacks()
    {
        BenzinProfile();

        for (const UpdateCallback& callback : m_UpdateCallbacks)
        {
            callback();
        }
    }

    void Scene::UploadMeshDrawsToGpu()
    {
        if (m_JointMeshDraws.empty())
        {
            m_JointMeshDraws.resize(m_MeshDraws.size());
        }

        for (uint32_t i = 0; i < m_MeshDraws.size(); ++i)
        {
            const MeshDraw& draw = m_MeshDraws[i];
            BenzinAssert(draw.m_MeshRangeIndex != g_MaxU32);

            const DirectX::XMMATRIX scaling = DirectX::XMMatrixScaling(draw.m_Scale, draw.m_Scale, draw.m_Scale);
            const DirectX::XMMATRIX rotation = DirectX::XMMatrixRotationX(draw.m_Rotation.x) * DirectX::XMMatrixRotationY(draw.m_Rotation.y) * DirectX::XMMatrixRotationZ(draw.m_Rotation.z);
            const DirectX::XMMATRIX translation = DirectX::XMMatrixTranslation(draw.m_Translation.x, draw.m_Translation.y, draw.m_Translation.z);

            joint::MeshDraw& jointDraw = m_JointMeshDraws[i];
            jointDraw.m_PrevLocalToWorld = jointDraw.m_LocalToWorld;
            jointDraw.m_LocalToWorld = scaling * rotation * translation;
        }

        m_MeshDrawBuffer = m_Device.GetTemporalLinearAllocator().AllocateAndWriteBuffer("Scene::MeshDraws", ToSpan(m_JointMeshDraws));
    }

}
