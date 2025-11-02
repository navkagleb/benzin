#include <benzin/config/bootstrap.hpp>
#include <benzin/engine/resource_helper.hpp>

#include <benzin/core/math.hpp>
#include <benzin/engine/mesh.hpp>

#include <shaders/joint/mesh_types.hpp>

#include <DirectXTex.h>
#include <meshoptimizer.h>

#define BENZIN_MESH_OPTIMIZATION_ENABLED 1

BenzinAllowDereferenceOperatorForEnum(joint::MeshletConsts);

namespace benzin
{

    void OptimizeMesh(Mesh& mesh)
    {
        // Optimize each mesh part separately

        std::vector<joint::MeshVertex> newVertices;
        std::vector<uint32_t> newIndices;

        for (MeshPart& part : mesh.m_Parts)
        {
            const auto vertices = ToSpan(mesh.m_Vertices.data() + part.m_VertexOffset, part.m_VertexCount);
            const auto indices = ToSpan(mesh.m_Indices.data() + part.m_IndexOffset, part.m_IndexCount);

            std::vector<uint32_t> remapIndices;
            remapIndices.resize(indices.size());

            const size_t vertexCount = meshopt_generateVertexRemap(
                remapIndices.data(),
                indices.data(),
                indices.size(),
                vertices.data(),
                vertices.size(),
                sizeof(joint::MeshVertex));

            std::vector<joint::MeshVertex> optVertices;
            std::vector<uint32_t> optIndices;

            optVertices.resize(vertexCount);
            optIndices.resize(indices.size());

            constexpr size_t vertexSizeInBytes = sizeof(joint::MeshVertex);

            meshopt_remapVertexBuffer(optVertices.data(), vertices.data(), vertices.size(), vertexSizeInBytes, remapIndices.data());
            meshopt_remapIndexBuffer(optIndices.data(), indices.data(), indices.size(), remapIndices.data());
            meshopt_optimizeVertexCache(optIndices.data(), optIndices.data(), optIndices.size(), optVertices.size());
            meshopt_optimizeOverdraw(optIndices.data(), optIndices.data(), optIndices.size(), &optVertices.front().m_Position.x, optVertices.size(), vertexSizeInBytes, 1.05f);
            meshopt_optimizeVertexFetch(optVertices.data(), optIndices.data(), optIndices.size(), optVertices.data(), optVertices.size(), vertexSizeInBytes);

            part.m_VertexOffset = (uint32_t)newVertices.size();
            part.m_VertexCount = (uint32_t)optVertices.size();
            part.m_IndexOffset = (uint32_t)newIndices.size();
            part.m_IndexCount = (uint32_t)optIndices.size();

            newVertices.append_range(optVertices);
            newIndices.append_range(optIndices);
        }

        mesh.m_Vertices = std::move(newVertices);
        mesh.m_Indices = std::move(newIndices);
    }

    void GenerateMeshlets(Mesh& mesh)
    {
        // NOTE: meshlet.triangle_offset is actually 'index offset' (not triangle) !!!

        BenzinAssert(mesh.m_Meshlets.empty());
        BenzinAssert(mesh.m_MeshletVertexIndices.empty());
        BenzinAssert(mesh.m_MeshletIndices.empty());

        constexpr size_t maxMeshletVertexCount = *joint::MeshletConsts::MaxVertexCount;
        constexpr size_t maxMeshletTriangleCount = *joint::MeshletConsts::MaxTriangleCount;
        constexpr float meshletConeWeight = 0.0f;

        for (MeshPart& part : mesh.m_Parts)
        {
            const auto vertices = ToSpan(mesh.m_Vertices.data() + part.m_VertexOffset, part.m_VertexCount);
            const auto indices = ToSpan(mesh.m_Indices.data() + part.m_IndexOffset, part.m_IndexCount);

            const size_t maxMeshletCount = meshopt_buildMeshletsBound(indices.size(), maxMeshletVertexCount, maxMeshletTriangleCount);

            std::vector<meshopt_Meshlet> meshoptMeshlets;
            std::vector<uint32_t> meshletVertexIndices;
            std::vector<uint8_t> meshletIndices;

            meshoptMeshlets.resize(maxMeshletCount);
            meshletVertexIndices.resize(maxMeshletCount * maxMeshletVertexCount);
            meshletIndices.resize(maxMeshletCount * maxMeshletTriangleCount * 3);

            const size_t meshletCount = meshopt_buildMeshlets(
                meshoptMeshlets.data(),
                meshletVertexIndices.data(),
                meshletIndices.data(),
                indices.data(),
                indices.size(),
                &vertices.front().m_Position.x,
                vertices.size(),
                sizeof(joint::MeshVertex),
                maxMeshletVertexCount,
                maxMeshletTriangleCount,
                meshletConeWeight);

            meshoptMeshlets.resize(meshletCount);

            const meshopt_Meshlet& lastMeshlet = meshoptMeshlets.back();
            meshletVertexIndices.resize(lastMeshlet.vertex_offset + lastMeshlet.vertex_count);
            meshletIndices.resize(lastMeshlet.triangle_offset + AlignUp(lastMeshlet.triangle_count * 3, 4u)); // Size must be multiple of 4

            std::vector<joint::Meshlet> meshlets;
            std::vector<joint::MeshletCullVolume> meshletCullVolumes;

            meshlets.reserve(meshletCount);
            meshletCullVolumes.reserve(meshletCount);

            for (const meshopt_Meshlet& meshoptMeshlet : meshoptMeshlets)
            {
                meshopt_optimizeMeshlet(
                    &meshletVertexIndices[meshoptMeshlet.vertex_offset],
                    &meshletIndices[meshoptMeshlet.triangle_offset],
                    meshoptMeshlet.triangle_count,
                    meshoptMeshlet.vertex_count);

                const meshopt_Bounds bounds = meshopt_computeMeshletBounds(
                    &meshletVertexIndices[meshoptMeshlet.vertex_offset],
                    &meshletIndices[meshoptMeshlet.triangle_offset],
                    meshoptMeshlet.triangle_count,
                    &vertices.front().m_Position.x,
                    vertices.size(),
                    sizeof(joint::MeshVertex));

                joint::Meshlet meshlet = {};
                meshlet.m_VertexOffset = meshoptMeshlet.vertex_offset;
                meshlet.m_VertexCount = meshoptMeshlet.vertex_count;
                meshlet.m_IndexOffset = meshoptMeshlet.triangle_offset;
                meshlet.m_TriangleCount = meshoptMeshlet.triangle_count;

                joint::MeshletCullVolume cullVolume = {};
                memcpy(&cullVolume.m_Center, &bounds.center, sizeof(DirectX::XMFLOAT3));
                cullVolume.m_Radius = bounds.radius;
                memcpy(&cullVolume.m_ConeApex, &bounds.cone_apex, sizeof(DirectX::XMFLOAT3));
                memcpy(&cullVolume.m_PackedAxisAndCutoff, &bounds.cone_axis_s8, sizeof(uint8_t) * 3);
                memcpy(((uint8_t*)&cullVolume.m_PackedAxisAndCutoff) + 3, &bounds.cone_cutoff_s8, sizeof(uint8_t));

                meshlets.push_back(meshlet);
                meshletCullVolumes.push_back(cullVolume);
            }

            part.m_MeshletOffset = (uint32_t)mesh.m_Meshlets.size();
            part.m_MeshletCount = (uint32_t)meshlets.size();
            part.m_MeshletVertexIndexOffset = (uint32_t)mesh.m_MeshletVertexIndices.size();
            part.m_MeshletVertexIndexCount = (uint32_t)meshletVertexIndices.size();
            part.m_MeshletIndexOffset = (uint32_t)mesh.m_MeshletIndices.size();
            part.m_MeshletIndexCount = (uint32_t)meshletIndices.size();

            mesh.m_Meshlets.append_range(std::move(meshlets));
            mesh.m_MeshletCullVolumes.append_range(std::move(meshletCullVolumes));
            mesh.m_MeshletVertexIndices.append_range(std::move(meshletVertexIndices));
            mesh.m_MeshletIndices.append_range(std::move(meshletIndices));
        }
    }

    bool SaveTextureArrayToDds(std::span<const std::string_view> fileNames, std::string_view outputFileName)
    {
        const uint32_t arraySize = (uint32_t)fileNames.size();

        std::vector<DirectX::ScratchImage> images;
        images.reserve(arraySize);

        for (const auto fileName : fileNames)
        {
            const std::filesystem::path filePath = GetTextureDir() / fileName;
            BenzinAssert(std::filesystem::exists(filePath));
            BenzinAssert(filePath.extension() == ".dds");

            DirectX::ScratchImage image;
            if (FAILED(DirectX::LoadFromDDSFile(filePath.c_str(), DirectX::DDS_FLAGS_NONE, nullptr, image)))
                return false;

            BenzinAssert(image.GetImageCount() == 1);
            BenzinAssert(image.GetMetadata().depth == 1);
            BenzinAssert(image.GetMetadata().arraySize == 1);

            images.push_back(std::move(image));
        }

        const size_t width = images[0].GetMetadata().width;
        const size_t height = images[0].GetMetadata().height;
        const DXGI_FORMAT format = images[0].GetMetadata().format;

#if BENZIN_IS_ASSERTS_ENABLED
        for (uint32_t i = 1; i < arraySize; ++i)
        {
            BenzinAssert(images[i].GetMetadata().width == width);
            BenzinAssert(images[i].GetMetadata().height == height);
            BenzinAssert(images[i].GetMetadata().format == format);
        }
#endif

        DirectX::ScratchImage textureArray;
        if (FAILED(textureArray.Initialize2D(format, width, height, arraySize, 1)))
            return false;

        for (uint32_t i = 0; i < arraySize; ++i)
        {
            const DirectX::Image* sourceImage = images[i].GetImage(0, 0, 0);
            const DirectX::Image* destImage = textureArray.GetImage(0, i, 0);

            BenzinAssert(sourceImage != nullptr && destImage != nullptr);
            memcpy(destImage->pixels, sourceImage->pixels, sourceImage->slicePitch);
        }

        const std::filesystem::path outputFilePath = GetTextureDir() / outputFileName;
        BenzinAssert(outputFilePath.extension() == ".dds");

        if (FAILED(DirectX::SaveToDDSFile(textureArray.GetImages(), textureArray.GetImageCount(), textureArray.GetMetadata(), DirectX::DDS_FLAGS_NONE, outputFilePath.c_str())))
            return false;

        BenzinTrace("Texture array saved to: {}", outputFilePath.string());

        return true;
    }

}
