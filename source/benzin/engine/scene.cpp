#include <benzin/config/bootstrap.hpp>
#include <benzin/engine/scene.hpp>

#include <benzin/core/math.hpp>
#include <benzin/core/profiler.hpp>
#include <benzin/engine/resource_helper.hpp>
#include <benzin/graphics/buffer.hpp>
#include <benzin/graphics/cmd_queue.hpp>
#include <benzin/graphics/device.hpp>
#include <benzin/graphics/gpu_heap.hpp>
#include <benzin/graphics/texture.hpp>

#include <shaders/joint/procedural_grass_resources.hpp>

namespace benzin
{

    static bool IsMatrixZero(const DirectX::XMFLOAT4X4& matrix)
    {
        for (uint32_t i = 0; i < 4; ++i)
        {
            for (uint32_t j = 0; j < 4; ++j)
            {
                if (matrix.m[i][j] != 0.0f)
                    return false;
            }
        }

        return true;
    }

    //

    Scene::Scene()
    {
        m_Materials.push_back(Material
        {
            .m_AlbedoFactor = { 1.0f, 0.0f, 1.0f, 1.0f },
        });
    }

    Scene::~Scene() = default;

    Scene::MeshGeometryRange Scene::AddMeshGeometry(
        const std::string& debugName,
        MeshGeometry&& geometry,
        std::vector<MeshDraw>&& meshDraws,
        std::vector<Material>&& materials,
        std::vector<TextureImage>&& textures)
    {
        BenzinTraceScopeTime("Scene::AddMeshGeometry - {}", debugName);

        {
            BenzinTraceScopeTime("{} mesh optimization + meshlet generation", debugName);

            OptimizeMeshGeometry(geometry);
            GenerateMeshlets(geometry);
        }

        const auto updateTextureIndex = [this](uint32_t& textureIndex)
        {
            if (!IsMaxUint(textureIndex))
            {
                textureIndex += (uint32_t)m_TextureImages.size();
            }
        };

        for (Material& material : materials)
        {
            updateTextureIndex(material.m_AlbedoTextureIndex);
            updateTextureIndex(material.m_NormalTextureIndex);
            updateTextureIndex(material.m_MetallicRoughnessTextureIndex);
            updateTextureIndex(material.m_EmissiveTextureIndex);
        }

        for (Mesh& mesh : geometry.m_Meshes)
        {
            mesh.m_VertexOffset += (uint32_t)m_Geometry.m_Vertices.size();

            const auto lods = ToMutSpan(mesh.m_Lods.data(), mesh.m_LodCount);
            for (MeshLod& lod : lods)
            {
                lod.m_IndexOffset += (uint32_t)m_Geometry.m_Indices.size();

                const auto meshletVertexIndices = ToMutSpan(geometry.m_MeshletVertexIndices.data() + lod.m_MeshletVertexIndexOffset, lod.m_MeshletVertexIndexCount);
                for (uint32_t& vertexIndex : meshletVertexIndices)
                {
                    vertexIndex += mesh.m_VertexOffset;
                }

                lod.m_MeshletVertexIndexOffset += (uint32_t)m_Geometry.m_MeshletVertexIndices.size();
                lod.m_MeshletIndexOffset += (uint32_t)m_Geometry.m_MeshletIndices.size();

                const auto meshlets = ToMutSpan(geometry.m_Meshlets.data() + lod.m_MeshletOffset, lod.m_MeshletCount);
                for (joint::Meshlet& meshlet : meshlets)
                {
                    meshlet.m_VertexOffset += lod.m_MeshletVertexIndexOffset;
                    meshlet.m_IndexOffset += lod.m_MeshletIndexOffset;
                }

                lod.m_MeshletOffset += (uint32_t)m_Geometry.m_Meshlets.size();
            }
        }

        for (MeshDraw& draw : meshDraws)
        {
            draw.m_MeshIndex += (uint32_t)m_Geometry.m_Meshes.size();

            if (IsMaxUint(draw.m_MaterialIndex))
            {
                draw.m_MaterialIndex = 0; // Fallback material index
            }
            else
            {
                draw.m_MaterialIndex += (uint32_t)m_Materials.size();
            }
        }

        MeshGeometryRange meshRange;
        meshRange.m_MeshDrawOffset = (uint32_t)m_MeshDraws.size();
        meshRange.m_MeshDrawCount = (uint32_t)meshDraws.size();

        m_Geometry.m_Vertices.append_range(std::move(geometry.m_Vertices));
        m_Geometry.m_Indices.append_range(std::move(geometry.m_Indices));
        m_Geometry.m_Meshes.append_range(std::move(geometry.m_Meshes));
        m_Geometry.m_Meshlets.append_range(std::move(geometry.m_Meshlets));
        m_Geometry.m_MeshletCullVolumes.append_range(std::move(geometry.m_MeshletCullVolumes));
        m_Geometry.m_MeshletVertexIndices.append_range(std::move(geometry.m_MeshletVertexIndices));
        m_Geometry.m_MeshletIndices.append_range(std::move(geometry.m_MeshletIndices));
        m_MeshDraws.append_range(std::move(meshDraws));
        m_Materials.append_range(std::move(materials));
        m_TextureImages.append_range(std::move(textures));

        return meshRange;
    }

    void Scene::UploadMeshGeometryToGpu(Device& device)
    {
        BenzinTraceScopeTime("Scene::UploadMeshGeometryToGpu");

        {
            m_Textures.reserve(m_TextureImages.size());

            for (TextureImage& textureImage : m_TextureImages)
            {
                m_Textures.push_back(device.GetPersistentDefaultAllocator().AllocateTexture([&textureImage](TextureCreation& creation)
                {
                    creation.m_DebugName = textureImage.m_DebugName;
                    creation.m_DxgiFormat = textureImage.m_DxgiFormat;
                    creation.m_Width = textureImage.m_Width;
                    creation.m_Height = textureImage.m_Height;
                    creation.m_MipCount = 1; // TODO: Mip generation
                }));
            }

            uint64_t uploadSizeInBytes = 0;
            for (const std::unique_ptr<Texture>& texture : m_Textures)
            {
                uploadSizeInBytes += AlignUp(texture->GetSizeInBytes(), D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT);
            }

            CopyCmdList& cmdList = device.GetGraphicsCmdQueue().GetCmdList(uploadSizeInBytes);
            for (uint32_t i = 0; i < m_Textures.size(); ++i)
            {
                cmdList.UploadToTexture(*m_Textures[i], m_TextureImages[i].m_PixelData);
            }
        }

        std::vector<joint::Mesh> jointMeshes;
        jointMeshes.reserve(m_Geometry.m_Meshes.size());

        for (const Mesh& mesh : m_Geometry.m_Meshes)
        {
            DirectX::BoundingSphere boundingSphere;
            DirectX::BoundingSphere::CreateFromPoints(
                boundingSphere,
                mesh.m_VertexCount,
                &m_Geometry.m_Vertices[mesh.m_VertexOffset].m_Position,
                sizeof(joint::MeshVertex));

            joint::Mesh& jointMesh = jointMeshes.emplace_back();
            jointMesh.m_Center = boundingSphere.Center;
            jointMesh.m_Radius = boundingSphere.Radius;
            jointMesh.m_VertexOffset = mesh.m_VertexOffset;
            jointMesh.m_LodCount = mesh.m_LodCount;

            for (uint32_t lodIndex = 0; lodIndex < mesh.m_LodCount; ++lodIndex)
            {
                const MeshLod& lod = mesh.m_Lods[lodIndex];

                joint::MeshLod& jointLod = jointMesh.m_Lods[lodIndex];
                jointLod.m_IndexOffset = lod.m_IndexOffset;
                jointLod.m_IndexCount = lod.m_IndexCount;
                jointLod.m_MeshletOffset = lod.m_MeshletOffset;
                jointLod.m_MeshletCount = lod.m_MeshletCount;
            }
        }

        const auto getTextureGpuHeapIndex = [this](uint32_t textureIndex)
        {
            return !IsMaxUint(textureIndex) ? m_Textures[textureIndex]->GetSrv().GetGpuHeapIndex() : g_MaxU32;
        };

        std::vector<joint::Material> jointMaterials;
        jointMaterials.reserve(m_Materials.size());

        for (const Material& material : m_Materials)
        {
            joint::Material& jointMaterial = jointMaterials.emplace_back();
            jointMaterial.m_AlbedoTextureHeapIndex = getTextureGpuHeapIndex(material.m_AlbedoTextureIndex);
            jointMaterial.m_NormalTextureHeapIndex = getTextureGpuHeapIndex(material.m_NormalTextureIndex);
            jointMaterial.m_MetallicRoughnessTextureHeapIndex = getTextureGpuHeapIndex(material.m_MetallicRoughnessTextureIndex);
            jointMaterial.m_EmissiveTextureHeapIndex = getTextureGpuHeapIndex(material.m_EmissiveTextureIndex);
            jointMaterial.m_AlbedoFactor = material.m_AlbedoFactor;
            jointMaterial.m_AlphaCutoff = material.m_AlphaCutoff;
            jointMaterial.m_NormalScale = material.m_NormalScale;
            jointMaterial.m_MetalnessFactor = material.m_MetalnessFactor;
            jointMaterial.m_RoughnessFactor = material.m_RoughnessFactor;
            jointMaterial.m_EmissiveFactor = material.m_EmissiveFactor;
        }

        GpuHeapLinearAllocator& allocator = device.GetPersistentDefaultAllocator();
        m_VertexBuffer = allocator.AllocateBuffer("Scene::Vertices", ToSpan(m_Geometry.m_Vertices));
        m_IndexBuffer = allocator.AllocateBuffer("Scene::Indices", ToSpan(m_Geometry.m_Indices), DXGI_FORMAT_R32_UINT);
        m_MeshBuffer = allocator.AllocateBuffer("Scene::Meshes", ToSpan(jointMeshes));
        m_MeshletBuffer = allocator.AllocateBuffer("Scene::Meshlets", ToSpan(m_Geometry.m_Meshlets));
        m_MeshletCullVolumeBuffer = allocator.AllocateBuffer("Scene::MeshletCullVolumes", ToSpan(m_Geometry.m_MeshletCullVolumes));
        m_MeshletVertexIndexBuffer = allocator.AllocateBuffer("Scene::MeshletVertexIndices", ToSpan(m_Geometry.m_MeshletVertexIndices), DXGI_FORMAT_R32_UINT);
        m_MeshletIndexBuffer = allocator.AllocateBuffer("Scene::MeshletIndices", ToSpan(m_Geometry.m_MeshletIndices), DXGI_FORMAT_R8_UINT);
        m_MaterialBuffer = allocator.AllocateBuffer("Scene::Materials", ToSpan(jointMaterials));

        if (!m_GrassPatches.empty())
        {
            m_GrassPatchBuffer = allocator.AllocateBuffer("Scene::GrassPatches", ToSpan(m_GrassPatches));
        }

        const uint64_t uploadSizeInBytes =
            m_VertexBuffer->GetSizeInBytes() +
            m_IndexBuffer->GetSizeInBytes() +
            m_MeshBuffer->GetSizeInBytes() +
            m_MeshletBuffer->GetSizeInBytes() +
            m_MeshletCullVolumeBuffer->GetSizeInBytes() +
            m_MeshletVertexIndexBuffer->GetSizeInBytes() +
            m_MeshletIndexBuffer->GetSizeInBytes() +
            m_MaterialBuffer->GetSizeInBytes() +
            (!m_GrassPatches.empty() ? m_GrassPatchBuffer->GetSizeInBytes() : 0);

        CopyCmdList& cmdList = device.GetGraphicsCmdQueue().GetCmdList(uploadSizeInBytes);
        cmdList.UploadToBuffer(*m_VertexBuffer, ToSpan(m_Geometry.m_Vertices));
        cmdList.UploadToBuffer(*m_IndexBuffer, ToSpan(m_Geometry.m_Indices));
        cmdList.UploadToBuffer(*m_MeshBuffer, ToSpan(jointMeshes));
        cmdList.UploadToBuffer(*m_MeshletBuffer, ToSpan(m_Geometry.m_Meshlets));
        cmdList.UploadToBuffer(*m_MeshletCullVolumeBuffer, ToSpan(m_Geometry.m_MeshletCullVolumes));
        cmdList.UploadToBuffer(*m_MeshletVertexIndexBuffer, ToSpan(m_Geometry.m_MeshletVertexIndices));
        cmdList.UploadToBuffer(*m_MeshletIndexBuffer, ToSpan(m_Geometry.m_MeshletIndices));
        cmdList.UploadToBuffer(*m_MaterialBuffer, ToSpan(jointMaterials));

        if (!m_GrassPatches.empty())
        {
            cmdList.UploadToBuffer(*m_GrassPatchBuffer, ToSpan(m_GrassPatches));
        }

        m_Geometry.m_Vertices.clear();
        m_Geometry.m_Indices.clear();
        m_Geometry.m_Meshes.clear();
        m_Geometry.m_Meshlets.clear();
        m_Geometry.m_MeshletCullVolumes.clear();
        m_Geometry.m_MeshletVertexIndices.clear();
        m_Geometry.m_MeshletIndices.clear();
        m_Materials.clear();
        m_TextureImages.clear();
        m_GrassPatches.clear();
    }

    void Scene::AllocateMeshDrawBuffers(Device& device)
    {
        for (const MeshGeometryDraw& geometryDraw : m_MeshGeometryDraws)
        {
            m_TotalMeshDrawCount += geometryDraw.m_MeshDrawCount;
        }

        for (uint32_t frameIndex = 0; frameIndex < BENZIN_FRAME_COUNT; ++frameIndex)
        {
            PerFrameResources& perFrameResources = m_PerFrameResources[frameIndex];
            perFrameResources.m_MeshDrawBuffer = device.GetPersistentGpuUploadAllocator().AllocateStructuredBuffer(
                std::format("Scene::MeshDrawBuffer{}", frameIndex),
                m_TotalMeshDrawCount,
                sizeof(joint::MeshDraw));
            perFrameResources.m_JointMeshDraws = ToMutSpan(
                (joint::MeshDraw*)perFrameResources.m_MeshDrawBuffer->GetCpuMappedData(),
                m_TotalMeshDrawCount);
        }
    }

    void Scene::UploadMeshDrawsToGpu(Device& device)
    {
        BenzinProfile();

        PerFrameResources& perFrameResources = m_PerFrameResources[device.GetActiveFrameIndex()];

        for (MeshGeometryDraw& geometryDraw : m_MeshGeometryDraws)
        {
            if (!perFrameResources.m_IsDirty && !geometryDraw.m_IsDirty)
                continue;

            const DirectX::XMMATRIX geometryLocalToWorld =
                DirectX::XMMatrixScaling(geometryDraw.m_Scale, geometryDraw.m_Scale, geometryDraw.m_Scale) *
                DirectX::XMMatrixRotationX(geometryDraw.m_Rotation.x) *
                DirectX::XMMatrixRotationY(geometryDraw.m_Rotation.y) *
                DirectX::XMMatrixRotationZ(geometryDraw.m_Rotation.z) *
                DirectX::XMMatrixTranslation(geometryDraw.m_Translation.x, geometryDraw.m_Translation.y, geometryDraw.m_Translation.z);

            const uint32_t drawBeginIndex = geometryDraw.m_MeshDrawOffset;
            const uint32_t drawEndIndex = geometryDraw.m_MeshDrawOffset + geometryDraw.m_MeshDrawCount;

            for (uint32_t drawIndex = drawBeginIndex; drawIndex < drawEndIndex; ++drawIndex)
            {
                const MeshDraw& draw = m_MeshDraws[drawIndex];
                joint::MeshDraw& jointDraw = perFrameResources.m_JointMeshDraws[drawIndex];

                jointDraw.m_MeshIndex = draw.m_MeshIndex;
                jointDraw.m_MaterialIndex = draw.m_MaterialIndex;

                const DirectX::XMMATRIX localToWorld = draw.m_ObjectToLocal * geometryLocalToWorld;

                if (IsMatrixZero(jointDraw.m_PrevLocalToWorld))
                {
                    DirectX::XMStoreFloat4x4(&jointDraw.m_PrevLocalToWorld, localToWorld);
                }
                else
                {
                    jointDraw.m_PrevLocalToWorld = jointDraw.m_LocalToWorld;
                }

                DirectX::XMStoreFloat4x4(&jointDraw.m_LocalToWorld, localToWorld);

                DirectX::XMFLOAT3 scales = {};
                scales.x = DirectX::XMVectorGetX(DirectX::XMVector3Length(localToWorld.r[0]));
                scales.y = DirectX::XMVectorGetX(DirectX::XMVector3Length(localToWorld.r[1]));
                scales.z = DirectX::XMVectorGetX(DirectX::XMVector3Length(localToWorld.r[2]));

                BenzinAssert(std::fabs(scales.x - scales.y) <= 1e-5f && std::fabs(scales.x - scales.z) <= 1e-5f, "Scale is not uniform");
                jointDraw.m_LocalToWorldScale = scales.x;
            }

            geometryDraw.m_IsDirty = false;
        }

        perFrameResources.m_IsDirty = false;
    }

}
