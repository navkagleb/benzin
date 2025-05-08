#include <benzin/config/bootstrap.hpp>
#include <benzin/engine/resource_helper.hpp>

#include <DirectXTex.h>
#include <meshoptimizer.h>

#include <shaders/joint/mesh_types.hpp>

#include <benzin/core/math.hpp>
#include <benzin/engine/mesh.hpp>

#define BENZIN_IS_MESH_OPTIMIZATION_ENABLED 1

BenzinEnableUnaryPlusForEnum(joint::MeshletConsts);

namespace benzin
{

    void OptimizeMesh(Mesh& mesh)
    {
        BenzinUnused(mesh);

#if BENZIN_IS_MESH_OPTIMIZATION_ENABLED
        // Optimize each mesh separately

        std::vector<joint::MeshVertex> newVertices;
        std::vector<uint32_t> newIndices;

        for (MeshDrawRange& drawRange : mesh.DrawRanges)
        {
            static_assert(sizeof(uint32_t) == sizeof(unsigned int));

            const size_t vertexSizeInBytes = sizeof(joint::MeshVertex);

            const auto drawVertices = mesh.GetDrawRangeVertices(drawRange);
            const auto drawIndices = mesh.GetDrawRangeIndices(drawRange);

            std::vector<uint32_t> remapIndices;
            remapIndices.resize(drawIndices.size());

            const size_t vertexCount = meshopt_generateVertexRemap(
                remapIndices.data(),
                drawIndices.data(),
                drawIndices.size(),
                drawVertices.data(),
                drawVertices.size(),
                sizeof(joint::MeshVertex)
            );

            std::vector<joint::MeshVertex> optVertices;
            std::vector<uint32_t> optIndices;

            optVertices.resize(vertexCount);
            optIndices.resize(drawIndices.size());

            meshopt_remapVertexBuffer(optVertices.data(), drawVertices.data(), drawVertices.size(), vertexSizeInBytes, remapIndices.data());
            meshopt_remapIndexBuffer(optIndices.data(), drawIndices.data(), drawIndices.size(), remapIndices.data());
            meshopt_optimizeVertexCache(optIndices.data(), optIndices.data(), optIndices.size(), optVertices.size());
            meshopt_optimizeOverdraw(optIndices.data(), optIndices.data(), optIndices.size(), &optVertices.front().Position.x, optVertices.size(), vertexSizeInBytes, 1.05f);
            meshopt_optimizeVertexFetch(optVertices.data(), optIndices.data(), optIndices.size(), optVertices.data(), optVertices.size(), vertexSizeInBytes);

            {
                // Update draw range

                drawRange.VertexRange.Offset = (uint32_t)newVertices.size();
                drawRange.VertexRange.Count = (uint32_t)optVertices.size();

                drawRange.IndexRange.Offset = (uint32_t)newIndices.size();
                drawRange.IndexRange.Count = (uint32_t)optIndices.size();
            }

            newVertices.append_range(optVertices);
            newIndices.append_range(optIndices);
        }

        mesh.Vertices = std::move(newVertices);
        mesh.Indices = std::move(newIndices);
#endif
    }

    void GenerateMeshlets(Mesh& mesh)
    {
        // NOTE: meshlet.triangle_offset is actually 'index offset' (not triangle) !!!

        BenzinAssert(mesh.Meshlets.empty());
        BenzinAssert(mesh.MeshletIndirectVertices.empty());
        BenzinAssert(mesh.MeshletIndices.empty());

        constexpr size_t maxMeshletVertexCount = +joint::MeshletConsts::MaxVertexCount;
        constexpr size_t maxMeshletTriangleCount = +joint::MeshletConsts::MaxTriangleCount;
        constexpr float meshletConeWeight = 0.0f;

        for (MeshDrawRange& drawRange : mesh.DrawRanges)
        {
            const auto drawVertices = mesh.GetDrawRangeVertices(drawRange);
            const auto drawIndices = mesh.GetDrawRangeIndices(drawRange);

            const size_t maxMeshletCount = meshopt_buildMeshletsBound(drawIndices.size(), maxMeshletVertexCount, maxMeshletTriangleCount);

            std::vector<meshopt_Meshlet> meshlets;
            std::vector<uint32_t> meshletIndirectVertices;
            std::vector<uint8_t> meshletIndices;

            meshlets.resize(maxMeshletCount);
            meshletIndirectVertices.resize(maxMeshletCount * maxMeshletVertexCount);
            meshletIndices.resize(maxMeshletCount * maxMeshletTriangleCount * 3);

            const size_t meshletCount = meshopt_buildMeshlets(
                meshlets.data(),
                meshletIndirectVertices.data(),
                meshletIndices.data(),
                drawIndices.data(),
                drawIndices.size(),
                &drawVertices.front().Position.x,
                drawVertices.size(),
                sizeof(decltype(drawVertices)::value_type),
                maxMeshletVertexCount,
                maxMeshletTriangleCount,
                meshletConeWeight
            );

            {
                // Trim buffers

                meshlets.resize(meshletCount);

                const meshopt_Meshlet& lastMeshlet = meshlets.back();

                meshletIndirectVertices.resize(lastMeshlet.vertex_offset + lastMeshlet.vertex_count);
                meshletIndices.resize(lastMeshlet.triangle_offset + AlignUp(lastMeshlet.triangle_count * 3, 4u)); // Size must be multiple of 4
            }


            for (const meshopt_Meshlet& meshlet : meshlets)
            {
                meshopt_optimizeMeshlet(
                    &meshletIndirectVertices[meshlet.vertex_offset],
                    &meshletIndices[meshlet.triangle_offset],
                    meshlet.triangle_count,
                    meshlet.vertex_count
                );
            }

            static_assert(sizeof(joint::Meshlet) == sizeof(meshopt_Meshlet));

            {
                // Update draw range

                drawRange.MeshletRange.Offset = (uint32_t)mesh.Meshlets.size();
                drawRange.MeshletRange.Count = (uint32_t)meshlets.size();

                drawRange.MeshletIndirectVertexRange.Offset = (uint32_t)mesh.MeshletIndirectVertices.size();
                drawRange.MeshletIndirectVertexRange.Count = (uint32_t)meshletIndirectVertices.size();

                drawRange.MeshletIndexRange.Offset = (uint32_t)mesh.MeshletIndices.size();
                drawRange.MeshletIndexRange.Count = (uint32_t)meshletIndices.size();
            }

            mesh.Meshlets.append_range(std::move(*decltype(&mesh.Meshlets)(&meshlets)));
            mesh.MeshletIndirectVertices.append_range(std::move(meshletIndirectVertices));
            mesh.MeshletIndices.append_range(std::move(meshletIndices));
        }
    }

    void GenerateBoundingSpheres(Mesh& mesh)
    {
        for (MeshDrawRange& drawRange : mesh.DrawRanges)
        {
            const auto drawVertices = mesh.GetDrawRangeVertices(drawRange);

            DirectX::BoundingSphere::CreateFromPoints(
                drawRange.BoundingSphere,
                drawVertices.size(),
                (DirectX::XMFLOAT3*)drawVertices.data(),
                sizeof(decltype(drawVertices)::value_type)
            );
        }
    }

    bool SaveTextureArrayToDds(std::span<const std::string_view> fileNames, std::string_view outputFileName)
    {
        const uint32_t arraySize = (uint32_t)fileNames.size();

        std::vector<DirectX::ScratchImage> images;
        images.reserve(arraySize);

        for (const auto fileName : fileNames)
        {
            const std::filesystem::path filePath = EngineConfig::s_TextureDir / fileName;
            BenzinAssert(std::filesystem::exists(filePath));
            BenzinAssert(filePath.extension() == ".dds");

            DirectX::ScratchImage image;
            if (FAILED(DirectX::LoadFromDDSFile(filePath.c_str(), DirectX::DDS_FLAGS_NONE, nullptr, image)))
            {
                return false;
            }

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
        {
            return false;
        }

        for (uint32_t i = 0; i < arraySize; ++i)
        {
            const DirectX::Image* sourceImage = images[i].GetImage(0, 0, 0);
            const DirectX::Image* destImage = textureArray.GetImage(0, i, 0);

            BenzinAssert(sourceImage != nullptr && destImage != nullptr);
            memcpy(destImage->pixels, sourceImage->pixels, sourceImage->slicePitch);
        }

        const std::filesystem::path outputFilePath = EngineConfig::s_TextureDir / outputFileName;
        BenzinAssert(outputFilePath.extension() == ".dds");

        if (FAILED(DirectX::SaveToDDSFile(
            textureArray.GetImages(),
            textureArray.GetImageCount(),
            textureArray.GetMetadata(),
            DirectX::DDS_FLAGS_NONE,
            outputFilePath.c_str()
        )))
        {
            return false;
        }

        BenzinTrace("Texture array saved to: {}", outputFilePath.string());

        return true;
    }

}
