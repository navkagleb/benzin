#include <benzin/config/bootstrap.hpp>
#include <benzin/engine/scene.hpp>

#include <benzin/core/math.hpp>
#include <benzin/core/profiler.hpp>
#include <benzin/engine/mesh.hpp>
#include <benzin/engine/resource_helper.hpp>
#include <benzin/engine/resource_loader.hpp>
#include <benzin/graphics/buffer.hpp>
#include <benzin/graphics/cmd_queue.hpp>
#include <benzin/graphics/device.hpp>
#include <benzin/graphics/gpu_heap.hpp>
#include <benzin/graphics/texture.hpp>

#include <shaders/joint/mesh_types.hpp>
#include <shaders/joint/procedural_grass_resources.hpp>

namespace benzin
{

    Scene::Scene(Device& device)
        : m_Device{ device }
    {
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
        }

        const auto textureOffset = (uint32_t)m_Textures.size();

        m_Textures.reserve(textureOffset + textures.size());
        m_TexturesData.reserve(textureOffset + textures.size());

        for (TextureImage& textureImage : textures)
        {
            TextureCreation creation;
            creation.m_DebugName = textureImage.m_DebugName;
            creation.m_DxgiFormat = textureImage.m_DxgiFormat;
            creation.m_Width = textureImage.m_Width;
            creation.m_Height = textureImage.m_Height;
            creation.m_MipCount = 1; // TODO: Mip generation

            m_Textures.push_back(std::make_unique<Texture>(m_Device, creation));
            m_TexturesData.push_back(std::move(textureImage.m_PixelData));
        }

        const auto updateTextureIndex = [this, textureOffset](uint32_t& textureIndex)
        {
            if (!IsMaxUint(textureIndex))
            {
                textureIndex = m_Textures[textureIndex + textureOffset]->GetSrv().GetGpuHeapIndex();
            }
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

            for (uint32_t i = part.m_MeshletVertexIndexOffset; i < part.m_MeshletVertexIndexOffset + part.m_MeshletVertexIndexCount; ++i)
            {
                uint32_t& vertexIndex = mesh.m_MeshletVertexIndices[i];
                vertexIndex += part.m_VertexOffset;
            }

            part.m_MeshletVertexIndexOffset += (uint32_t)m_MeshletVertexIndices.size();
            part.m_MeshletIndexOffset += (uint32_t)m_MeshletIndices.size();

            for (uint32_t i = part.m_MeshletOffset; i < part.m_MeshletOffset + part.m_MeshletCount; ++i)
            {
                joint::Meshlet& meshlet = mesh.m_Meshlets[i];
                meshlet.m_VertexOffset += part.m_MeshletVertexIndexOffset;
                meshlet.m_IndexOffset += part.m_MeshletIndexOffset;
            }

            part.m_MeshletOffset += (uint32_t)m_Meshlets.size();
        }

        for (MeshDrawPart& meshDrawPart : meshDrawParts)
        {
            meshDrawPart.m_PartIndex += (uint32_t)m_MeshParts.size();
            
            if (IsMaxUint(meshDrawPart.m_MaterialIndex))
            {
                meshDrawPart.m_MaterialIndex = 0; // Fallback material index
            }
            else
            {
                meshDrawPart.m_MaterialIndex += (uint32_t)m_Materials.size();
            }
        }

        MeshRange meshRange;
        meshRange.m_DrawPartOffset = (uint32_t)m_MeshDrawParts.size();
        meshRange.m_DrawPartCount = (uint32_t)meshDrawParts.size();

        m_MeshRangeMap[debugName] = (uint32_t)m_MeshRanges.size();
        m_MeshRanges.push_back(meshRange);

        m_Vertices.append_range(std::move(mesh.m_Vertices));
        m_Indices.append_range(std::move(mesh.m_Indices));
        m_MeshParts.append_range(std::move(mesh.m_Parts));
        m_MeshDrawParts.append_range(std::move(meshDrawParts));
        m_Meshlets.append_range(std::move(mesh.m_Meshlets));
        m_MeshletCullVolumes.append_range(std::move(mesh.m_MeshletCullVolumes));
        m_MeshletVertexIndices.append_range(std::move(mesh.m_MeshletVertexIndices));
        m_MeshletIndices.append_range(std::move(mesh.m_MeshletIndices));
        m_Materials.append_range(std::move(materials));
    }

    void Scene::UploadToGpu()
    {
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

        std::vector<DrawIndirectCmd> drawIndirectCmds;
        drawIndirectCmds.reserve(m_MeshDrawParts.size());
        
        for (uint32_t drawIndex = 0; drawIndex < m_JointMeshDraws.size(); ++drawIndex)
        {
            const joint::MeshDraw& draw = m_JointMeshDraws[drawIndex];
            const MeshPart& part = m_MeshParts[draw.m_PartIndex];

            DrawIndirectCmd& cmd = drawIndirectCmds.emplace_back();
            cmd.m_DrawIndex = drawIndex;
            cmd.m_D3D12Cmd.IndexCountPerInstance = part.m_IndexCount;
            cmd.m_D3D12Cmd.InstanceCount = 1;
            cmd.m_D3D12Cmd.StartIndexLocation = part.m_IndexOffset;
            cmd.m_D3D12Cmd.BaseVertexLocation = part.m_VertexOffset;
            cmd.m_D3D12Cmd.StartInstanceLocation = 0;
        }

        std::vector<DispatchMeshIndirectCmd> dispatchMeshIndirectCmds;
        dispatchMeshIndirectCmds.reserve(m_MeshDrawParts.size());

        for (uint32_t drawIndex = 0; drawIndex < m_JointMeshDraws.size(); ++drawIndex)
        {
            const joint::MeshDraw& draw = m_JointMeshDraws[drawIndex];
            const MeshPart& part = m_MeshParts[draw.m_PartIndex];

            DispatchMeshIndirectCmd& cmd = dispatchMeshIndirectCmds.emplace_back();
            cmd.m_DrawIndex = drawIndex;
            cmd.m_MeshletOffset = part.m_MeshletOffset;
            cmd.m_MeshletCount = part.m_MeshletCount;
            cmd.m_D3D12Cmd.ThreadGroupCountX = DivideUp(part.m_MeshletCount, (uint32_t)joint::MeshletConsts::AsGroupSize);
            cmd.m_D3D12Cmd.ThreadGroupCountY = 1;
            cmd.m_D3D12Cmd.ThreadGroupCountZ = 1;
        }

        GpuHeapLinearAllocator& allocator = m_Device.GetPersistentDefaultAllocator();
        m_VertexBuffer = allocator.AllocateBuffer("Scene::VertexBuffer", ToSpan(m_Vertices));
        m_IndexBuffer = allocator.AllocateBuffer("Scene::IndexBuffer", ToSpan(m_Indices), DXGI_FORMAT_R32_UINT);
        m_MeshletBuffer = allocator.AllocateBuffer("Scene::MeshletsBuffer", ToSpan(m_Meshlets));
        m_MeshletCullVolumeBuffer = allocator.AllocateBuffer("Scene::MeshletCullVolumeBuffer", ToSpan(m_MeshletCullVolumes));
        m_MeshletVertexIndexBuffer = allocator.AllocateBuffer("Scene::MeshletVertexIndexBuffer", ToSpan(m_MeshletVertexIndices), DXGI_FORMAT_R32_UINT);
        m_MeshletIndexBuffer = allocator.AllocateBuffer("Scene::MeshletIndexBuffer", ToSpan(m_MeshletIndices), DXGI_FORMAT_R8_UINT);
        m_MaterialBuffer = allocator.AllocateBuffer("Scene::MaterialBuffer", ToSpan(jointMaterials));
        m_DrawIndirectCmdBuffer = allocator.AllocateBuffer("Scene::DrawIndirectCmdBuffer", ToSpan(drawIndirectCmds));
        m_DispatchMeshIndirectCmdBuffer = allocator.AllocateBuffer("Scene::DispatchMeshIndirectCmdBuffer", ToSpan(dispatchMeshIndirectCmds));

        const uint64_t uploadSizeInBytes =
            m_VertexBuffer->GetSizeInBytes() +
            m_IndexBuffer->GetSizeInBytes() +
            m_MeshletBuffer->GetSizeInBytes() +
            m_MeshletCullVolumeBuffer->GetSizeInBytes() +
            m_MeshletVertexIndexBuffer->GetSizeInBytes() +
            m_MeshletIndexBuffer->GetSizeInBytes() +
            m_MaterialBuffer->GetSizeInBytes() +
            m_DrawIndirectCmdBuffer->GetSizeInBytes() +
            m_DispatchMeshIndirectCmdBuffer->GetSizeInBytes();

        CopyCmdList& cmdList = m_Device.GetGraphicsCmdQueue().GetCmdList(uploadSizeInBytes);
        cmdList.UploadToBuffer(*m_VertexBuffer, ToSpan(m_Vertices));
        cmdList.UploadToBuffer(*m_IndexBuffer, ToSpan(m_Indices));
        cmdList.UploadToBuffer(*m_MeshletBuffer, ToSpan(m_Meshlets));
        cmdList.UploadToBuffer(*m_MeshletCullVolumeBuffer, ToSpan(m_MeshletCullVolumes));
        cmdList.UploadToBuffer(*m_MeshletVertexIndexBuffer, ToSpan(m_MeshletVertexIndices));
        cmdList.UploadToBuffer(*m_MeshletIndexBuffer, ToSpan(m_MeshletIndices));
        cmdList.UploadToBuffer(*m_MaterialBuffer, ToSpan(jointMaterials));
        cmdList.UploadToBuffer(*m_DrawIndirectCmdBuffer, ToSpan(drawIndirectCmds));
        cmdList.UploadToBuffer(*m_DispatchMeshIndirectCmdBuffer, ToSpan(dispatchMeshIndirectCmds));
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
        BenzinProfile();

        if (m_JointMeshDraws.empty())
        {
            uint32_t drawCount = 0;
            for (const MeshDraw& draw : m_MeshDraws)
            {
                drawCount += m_MeshRanges[draw.m_MeshRangeIndex].m_DrawPartCount;
            }

            m_JointMeshDraws.resize(drawCount);

            m_MeshDrawBuffer = m_Device.GetPersistentGpuUploadAllocator().AllocateBuffer(
                "Scene::MeshDraws",
                ToSpan(m_JointMeshDraws));
        }

        uint32_t jointDrawIndex = 0;
        for (const MeshDraw& draw : m_MeshDraws)
        {
            const uint32_t drawPartOffset = m_MeshRanges[draw.m_MeshRangeIndex].m_DrawPartOffset;
            const uint32_t drawPartCount = m_MeshRanges[draw.m_MeshRangeIndex].m_DrawPartCount;

            const auto drawParts = ToSpan(m_MeshDrawParts.data() + drawPartOffset, drawPartCount);
            for (const MeshDrawPart& drawPart : drawParts)
            {
                const DirectX::XMMATRIX scaling = DirectX::XMMatrixScaling(draw.m_Scale, draw.m_Scale, draw.m_Scale);
                const DirectX::XMMATRIX rotation = DirectX::XMMatrixRotationX(draw.m_Rotation.x) * DirectX::XMMatrixRotationY(draw.m_Rotation.y) * DirectX::XMMatrixRotationZ(draw.m_Rotation.z);
                const DirectX::XMMATRIX translation = DirectX::XMMatrixTranslation(draw.m_Translation.x, draw.m_Translation.y, draw.m_Translation.z);

                joint::MeshDraw& jointDraw = m_JointMeshDraws[jointDrawIndex++];
                jointDraw.m_PrevLocalToWorld = jointDraw.m_LocalToWorld;
                jointDraw.m_LocalToWorld = drawPart.m_ObjectToLocal * (scaling * rotation * translation);
                jointDraw.m_MaterialIndex = drawPart.m_MaterialIndex;
                jointDraw.m_PartIndex = drawPart.m_PartIndex;
            }
        }

        BufferWriter writer = MakeBufferWriter(*m_MeshDrawBuffer);
        writer.WriteArray(ToSpan(m_JointMeshDraws));
    }

}
