#include <benzin/config/bootstrap.hpp>
#include <benzin/engine/resource_helper.hpp>

#include <benzin/core/math.hpp>
#include <benzin/engine/mesh.hpp>

#include <shaders/joint/mesh_types.hpp>

#include <DirectXTex.h>
#include <meshoptimizer.h>

BenzinAllowDereferenceOperatorForEnum(joint::MeshletConsts);

namespace benzin
{

    void OptimizeMeshGeometry(MeshGeometry& geometry)
    {
        // Optimize each mesh separately

        std::vector<joint::MeshVertex> newVertices;
        std::vector<uint32_t> newIndices;

        for (Mesh& mesh : geometry.m_Meshes)
        {
            BenzinAssert(mesh.m_LodCount == 1);

            const auto vertices = ToSpan(geometry.m_Vertices.data() + mesh.m_VertexOffset, mesh.m_VertexCount);
            const auto indices = ToSpan(geometry.m_Indices.data() + mesh.m_Lods[0].m_IndexOffset, mesh.m_Lods[0].m_IndexCount);

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

            // meshopt_optimizeOverdraw(optIndices.data(), optIndices.data(), optIndices.size(), &optVertices.front().m_Position.x, optVertices.size(), vertexSizeInBytes, 1.05f);
            meshopt_optimizeVertexFetch(optVertices.data(), optIndices.data(), optIndices.size(), optVertices.data(), optVertices.size(), vertexSizeInBytes);

            mesh.m_VertexOffset = (uint32_t)newVertices.size();
            mesh.m_VertexCount = (uint32_t)optVertices.size();

            std::vector<uint32_t> lodIndices = optIndices;

            mesh.m_LodCount = 0;
            while (mesh.m_LodCount < 8)
            {
                MeshLod& lod = mesh.m_Lods[mesh.m_LodCount++];
                lod.m_IndexOffset = (uint32_t)newIndices.size();
                lod.m_IndexCount = (uint32_t)lodIndices.size();

                meshopt_optimizeVertexCache(lodIndices.data(), lodIndices.data(), lodIndices.size(), optVertices.size());
                newIndices.append_range(lodIndices);

                if (mesh.m_LodCount == 8)
                    break;

                const size_t targetIndexCount = (size_t)(lodIndices.size() * 0.75f);
                const float error = 1.0f;
                const size_t lodIndexCount = meshopt_simplify(
                    lodIndices.data(),
                    lodIndices.data(),
                    lodIndices.size(),
                    &optVertices.front().m_Position.x,
                    optVertices.size(),
                    vertexSizeInBytes,
                    targetIndexCount,
                    error);

                BenzinAssert(lodIndexCount <= lodIndices.size());

                // We've reached the error bound
                if (lodIndexCount == lodIndices.size())
                    break;

                lodIndices.resize(lodIndexCount);
            }

            newVertices.append_range(optVertices);
        }

        geometry.m_Vertices = std::move(newVertices);
        geometry.m_Indices = std::move(newIndices);
    }

    void GenerateMeshlets(MeshGeometry& geometry)
    {
        // NOTE: meshlet.triangle_offset is actually 'index offset' (not triangle) !!!

        BenzinAssert(geometry.m_Meshlets.empty());
        BenzinAssert(geometry.m_MeshletVertexIndices.empty());
        BenzinAssert(geometry.m_MeshletIndices.empty());

        constexpr size_t maxMeshletVertexCount = *joint::MeshletConsts::MaxVertexCount;
        constexpr size_t maxMeshletTriangleCount = *joint::MeshletConsts::MaxTriangleCount;
        constexpr float meshletConeWeight = 0.0f;

        for (Mesh& mesh : geometry.m_Meshes)
        {
            const auto vertices = ToSpan(geometry.m_Vertices.data() + mesh.m_VertexOffset, mesh.m_VertexCount);

            const auto lods = ToMutSpan(mesh.m_Lods.data(), mesh.m_LodCount);
            for (MeshLod& lod : lods)
            {
                const auto indices = ToSpan(geometry.m_Indices.data() + lod.m_IndexOffset, lod.m_IndexCount);

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

                lod.m_MeshletOffset = (uint32_t)geometry.m_Meshlets.size();
                lod.m_MeshletCount = (uint32_t)meshlets.size();
                lod.m_MeshletVertexIndexOffset = (uint32_t)geometry.m_MeshletVertexIndices.size();
                lod.m_MeshletVertexIndexCount = (uint32_t)meshletVertexIndices.size();
                lod.m_MeshletIndexOffset = (uint32_t)geometry.m_MeshletIndices.size();
                lod.m_MeshletIndexCount = (uint32_t)meshletIndices.size();

                geometry.m_Meshlets.append_range(std::move(meshlets));
                geometry.m_MeshletCullVolumes.append_range(std::move(meshletCullVolumes));
                geometry.m_MeshletVertexIndices.append_range(std::move(meshletVertexIndices));
                geometry.m_MeshletIndices.append_range(std::move(meshletIndices));
            }
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
