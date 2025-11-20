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

#include <shaders/joint/mesh_types.hpp>
#include <shaders/joint/procedural_grass_resources.hpp>

namespace benzin
{

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
            mesh.m_IndexOffset += (uint32_t)m_Geometry.m_Indices.size();

            const auto meshletVertexIndices = ToMutSpan(geometry.m_MeshletVertexIndices.data() + mesh.m_MeshletVertexIndexOffset, mesh.m_MeshletVertexIndexCount);
            for (uint32_t& vertexIndex : meshletVertexIndices)
            {
                vertexIndex += mesh.m_VertexOffset;
            }

            mesh.m_MeshletVertexIndexOffset += (uint32_t)m_Geometry.m_MeshletVertexIndices.size();
            mesh.m_MeshletIndexOffset += (uint32_t)m_Geometry.m_MeshletIndices.size();

            const auto meshlets = ToMutSpan(geometry.m_Meshlets.data() + mesh.m_MeshletOffset, mesh.m_MeshletCount);
            for (joint::Meshlet& meshlet : meshlets)
            {
                meshlet.m_VertexOffset += mesh.m_MeshletVertexIndexOffset;
                meshlet.m_IndexOffset += mesh.m_MeshletIndexOffset;
            }

            mesh.m_MeshletOffset += (uint32_t)m_Geometry.m_Meshlets.size();
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
        BenzinTraceScopeTime("Scene::UploadToGpu");

        {
            m_Textures.reserve(m_TextureImages.size());

            for (TextureImage& textureImage : m_TextureImages)
            {
                std::unique_ptr texture = device.GetPersistentDefaultAllocator().AllocateTexture([&textureImage](TextureCreation& creation)
                {
                    creation.m_DebugName = textureImage.m_DebugName;
                    creation.m_DxgiFormat = textureImage.m_DxgiFormat;
                    creation.m_Width = textureImage.m_Width;
                    creation.m_Height = textureImage.m_Height;
                    creation.m_MipCount = 1; // TODO: Mip generation
                });

                m_Textures.push_back(std::move(texture));
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
            jointMesh.m_VertexOffset = mesh.m_VertexOffset;
            jointMesh.m_IndexOffset = mesh.m_IndexOffset;
            jointMesh.m_IndexCount = mesh.m_IndexCount;
            jointMesh.m_Center = boundingSphere.Center;
            jointMesh.m_Radius = boundingSphere.Radius;
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

        uint32_t meshDispatchCount = 0;
        for (const joint::MeshDraw& draw : m_JointMeshDraws)
        {
            meshDispatchCount += m_Geometry.m_Meshes[draw.m_MeshIndex].m_MeshletCount;
        }

        std::vector<joint::MeshDispatch> meshDispatches;
        meshDispatches.reserve(meshDispatchCount);

        for (uint32_t drawIndex = 0; drawIndex < m_JointMeshDraws.size(); ++drawIndex)
        {
            const joint::MeshDraw& draw = m_JointMeshDraws[drawIndex];
            const Mesh& mesh = m_Geometry.m_Meshes[draw.m_MeshIndex];

            for (uint32_t meshletIndex = mesh.m_MeshletOffset; meshletIndex < mesh.m_MeshletOffset + mesh.m_MeshletCount; ++meshletIndex)
            {
                joint::MeshDispatch& dispatch = meshDispatches.emplace_back();
                dispatch.m_MeshDrawIndex = drawIndex;
                dispatch.m_MeshletIndex = meshletIndex;
            }
        }

        GpuHeapLinearAllocator& allocator = device.GetPersistentDefaultAllocator();
        m_VertexBuffer = allocator.AllocateBuffer("Scene::VertexBuffer", ToSpan(m_Geometry.m_Vertices));
        m_IndexBuffer = allocator.AllocateBuffer("Scene::IndexBuffer", ToSpan(m_Geometry.m_Indices), DXGI_FORMAT_R32_UINT);
        m_MeshBuffer = allocator.AllocateBuffer("Scene::MeshBuffer", ToSpan(jointMeshes));
        m_MeshletBuffer = allocator.AllocateBuffer("Scene::MeshletsBuffer", ToSpan(m_Geometry.m_Meshlets));
        m_MeshletCullVolumeBuffer = allocator.AllocateBuffer("Scene::MeshletCullVolumeBuffer", ToSpan(m_Geometry.m_MeshletCullVolumes));
        m_MeshletVertexIndexBuffer = allocator.AllocateBuffer("Scene::MeshletVertexIndexBuffer", ToSpan(m_Geometry.m_MeshletVertexIndices), DXGI_FORMAT_R32_UINT);
        m_MeshletIndexBuffer = allocator.AllocateBuffer("Scene::MeshletIndexBuffer", ToSpan(m_Geometry.m_MeshletIndices), DXGI_FORMAT_R8_UINT);
        m_MaterialBuffer = allocator.AllocateBuffer("Scene::MaterialBuffer", ToSpan(jointMaterials));
        m_MeshDispatchBuffer = allocator.AllocateBuffer("Scene::MeshDispatchBuffer", ToSpan(meshDispatches));

        const uint64_t uploadSizeInBytes =
            m_VertexBuffer->GetSizeInBytes() +
            m_IndexBuffer->GetSizeInBytes() +
            m_MeshBuffer->GetSizeInBytes() +
            m_MeshletBuffer->GetSizeInBytes() +
            m_MeshletCullVolumeBuffer->GetSizeInBytes() +
            m_MeshletVertexIndexBuffer->GetSizeInBytes() +
            m_MeshletIndexBuffer->GetSizeInBytes() +
            m_MaterialBuffer->GetSizeInBytes() +
            m_MeshDispatchBuffer->GetSizeInBytes();

        CopyCmdList& cmdList = device.GetGraphicsCmdQueue().GetCmdList(uploadSizeInBytes);
        cmdList.UploadToBuffer(*m_VertexBuffer, ToSpan(m_Geometry.m_Vertices));
        cmdList.UploadToBuffer(*m_IndexBuffer, ToSpan(m_Geometry.m_Indices));
        cmdList.UploadToBuffer(*m_MeshBuffer, ToSpan(jointMeshes));
        cmdList.UploadToBuffer(*m_MeshletBuffer, ToSpan(m_Geometry.m_Meshlets));
        cmdList.UploadToBuffer(*m_MeshletCullVolumeBuffer, ToSpan(m_Geometry.m_MeshletCullVolumes));
        cmdList.UploadToBuffer(*m_MeshletVertexIndexBuffer, ToSpan(m_Geometry.m_MeshletVertexIndices));
        cmdList.UploadToBuffer(*m_MeshletIndexBuffer, ToSpan(m_Geometry.m_MeshletIndices));
        cmdList.UploadToBuffer(*m_MaterialBuffer, ToSpan(jointMaterials));
        cmdList.UploadToBuffer(*m_MeshDispatchBuffer, ToSpan(meshDispatches));
    }

    void Scene::UploadMeshDrawsToGpu(Device& device)
    {
        BenzinProfile();

        if (m_JointMeshDraws.empty())
        {
            uint32_t drawCount = 0;
            for (const MeshGeometryDraw& geometryDraw : m_MeshGeometryDraws)
            {
                drawCount += geometryDraw.m_MeshDrawCount;
            }

            m_JointMeshDraws.resize(drawCount);
            m_MeshDrawBuffer = device.GetPersistentGpuUploadAllocator().AllocateBuffer("Scene::MeshDraws", ToSpan(m_JointMeshDraws));
        }

        uint32_t jointDrawIndex = 0;
        for (const MeshGeometryDraw& geometryDraw : m_MeshGeometryDraws)
        {
            const auto draws = ToSpan(m_MeshDraws.data() + geometryDraw.m_MeshDrawOffset, geometryDraw.m_MeshDrawCount);
            for (const MeshDraw& draw : draws)
            {
                const DirectX::XMMATRIX scaling = DirectX::XMMatrixScaling(geometryDraw.m_Scale, geometryDraw.m_Scale, geometryDraw.m_Scale);
                const DirectX::XMMATRIX rotation = DirectX::XMMatrixRotationX(geometryDraw.m_Rotation.x) * DirectX::XMMatrixRotationY(geometryDraw.m_Rotation.y) * DirectX::XMMatrixRotationZ(geometryDraw.m_Rotation.z);
                const DirectX::XMMATRIX translation = DirectX::XMMatrixTranslation(geometryDraw.m_Translation.x, geometryDraw.m_Translation.y, geometryDraw.m_Translation.z);

                joint::MeshDraw& jointDraw = m_JointMeshDraws[jointDrawIndex++];
                jointDraw.m_PrevLocalToWorld = jointDraw.m_LocalToWorld;
                jointDraw.m_LocalToWorld = draw.m_ObjectToLocal * (scaling * rotation * translation);
                jointDraw.m_MeshIndex = draw.m_MeshIndex;
                jointDraw.m_MaterialIndex = draw.m_MaterialIndex;

                DirectX::XMFLOAT3 scales = {};
                scales.x = DirectX::XMVectorGetX(DirectX::XMVector3Length(jointDraw.m_LocalToWorld.r[0]));
                scales.y = DirectX::XMVectorGetX(DirectX::XMVector3Length(jointDraw.m_LocalToWorld.r[1]));
                scales.z = DirectX::XMVectorGetX(DirectX::XMVector3Length(jointDraw.m_LocalToWorld.r[2]));

                BenzinAssert(std::fabs(scales.x - scales.y) <= 1e-5f && std::fabs(scales.x - scales.z) <= 1e-5f, "Scale is not uniform");
                jointDraw.m_LocalToWorldScale = scales.x;
            }
        }

        BufferWriter writer = MakeBufferWriter(*m_MeshDrawBuffer);
        writer.WriteArray(ToSpan(m_JointMeshDraws));
    }

}
