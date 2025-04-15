#include <benzin/config/bootstrap.hpp>
#include <benzin/engine/gltf_reader.hpp>

#include <stb_image.h>
#include <tiny_gltf.h>

#include <shaders/joint/mesh_types.hpp>

#include <benzin/core/engine_math.hpp>
#include <benzin/engine/mesh.hpp>

namespace benzin
{

    static DirectX::XMMATRIX ParseNodeTransform(const tinygltf::Node& gltfNode, const DirectX::XMMATRIX& parentNodeTransform)
    {
        DirectX::XMMATRIX nodeTransform = DirectX::XMMatrixIdentity();

        if (!gltfNode.matrix.empty())
        {
            // Actually 'gltfNode.matrix' is already in row-major order. So there is no need to transpose it
            // Ref: glTF docs: https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#transformations
            // Ref: Row-major vs column-major matrices: https://gamedev.stackexchange.com/questions/153816/why-do-these-directxmath-functions-seem-like-they-return-column-major-matrics

            BenzinAssert(gltfNode.matrix.size() == 16);

            for (uint32_t i = 0; i < 4; ++i)
            {
                nodeTransform.r[i] = DirectX::XMVECTOR
                {
                    (float)gltfNode.matrix[0 + i * 4],
                    (float)gltfNode.matrix[1 + i * 4],
                    (float)gltfNode.matrix[2 + i * 4],
                    (float)gltfNode.matrix[3 + i * 4],
                };
            }
        }
        else
        {
            if (!gltfNode.rotation.empty())
            {
                BenzinAssert(gltfNode.rotation.size() == 4);
                const DirectX::XMFLOAT4 rotation
                {
                    (float)gltfNode.rotation[0],
                    (float)gltfNode.rotation[1],
                    (float)gltfNode.rotation[2],
                    (float)gltfNode.rotation[3],
                };

                nodeTransform *= DirectX::XMMatrixRotationQuaternion(DirectX::XMLoadFloat4(&rotation));
            }

            if (!gltfNode.scale.empty())
            {
                BenzinAssert(gltfNode.scale.size() == 3);
                const DirectX::XMFLOAT3 scale
                {
                    (float)gltfNode.scale[0],
                    (float)gltfNode.scale[1],
                    (float)gltfNode.scale[2],
                };

                nodeTransform *= DirectX::XMMatrixScalingFromVector(DirectX::XMLoadFloat3(&scale));
            }

            if (!gltfNode.translation.empty())
            {
                BenzinAssert(gltfNode.translation.size() == 3);
                const DirectX::XMFLOAT3 translation
                {
                    (float)gltfNode.translation[0],
                    (float)gltfNode.translation[1],
                    (float)gltfNode.translation[2],
                };

                nodeTransform *= DirectX::XMMatrixTranslationFromVector(DirectX::XMLoadFloat3(&translation));
            }
        }

        return nodeTransform * parentNodeTransform;
    }

    //

    GltfReader::GltfReader()
    {
        MakeUniquePtr(m_GltfContext);
        ResetState();
    }

    GltfReader::~GltfReader() = default;

    bool GltfReader::ReadFromFile(std::string_view fileName, MeshResource& outMesh)
    {
        BenzinAssert(m_GltfContext.get() != nullptr && m_GltfModel.get() != nullptr);
        BenzinAssert(m_OutMesh == nullptr);
        m_OutMesh = &outMesh;

        const std::filesystem::path filePath = EngineConfig::s_ModelDir / fileName;
        BenzinAssert(std::filesystem::exists(filePath));
        BenzinAssert(filePath.extension() == ".glb" || filePath.extension() == ".gltf");

        std::string error;
        std::string warning;
        bool isFileLoadingSucceed = false;

        const std::string filePathStr = filePath.string();

        {
            BenzinLogTimeOnScopeExit("GLTF Reader: LoadFromFile {}", filePathStr);

            if (filePath.extension() == ".glb")
            {
                isFileLoadingSucceed = m_GltfContext->LoadBinaryFromFile(m_GltfModel.get(), &error, &warning, filePathStr);
            }
            else if (filePath.extension() == ".gltf")
            {
                isFileLoadingSucceed = m_GltfContext->LoadASCIIFromFile(m_GltfModel.get(), &error, &warning, filePathStr);
            }
        }

        if (!warning.empty())
        {
            BenzinWarning("GLTF Reader: {}", warning);
            return false;
        }
        else if (!error.empty())
        {
            BenzinWarning("GLTF Reader: {}", warning);
            return false;
        }
        else if (!isFileLoadingSucceed)
        {
            BenzinWarning("GLTF Reader: Failed to load from {}. There is no specific error", filePathStr);
            return false;
        }

        m_OutMesh->DebugName = CutExtension(fileName);
        ParseGltfMeshes();
        ParseGltfNodes();
        ParseGltfMaterials();
        ParseGltfTextures();

        ResetState();

        return true;
    }

    template <typename T>
    std::span<const T> GltfReader::ParseGltfAccessor(int gltfaAccessorIndex)
    {
        if (gltfaAccessorIndex == -1)
        {
            return {};
        }

        const tinygltf::Accessor& gltfAccessor = m_GltfModel->accessors[gltfaAccessorIndex];
        const tinygltf::BufferView& gltfBufferView = m_GltfModel->bufferViews[gltfAccessor.bufferView];
        const tinygltf::Buffer& gltfBuffer = m_GltfModel->buffers[gltfBufferView.buffer];

        const Bytes64 offset = gltfBufferView.byteOffset + gltfAccessor.byteOffset;

        return std::span
        {
            reinterpret_cast<const T*>(gltfBuffer.data.data() + offset),
            gltfAccessor.count
        };
    }

    template <std::integral IndexType>
    MeshData GltfReader::ParseGltfPrimitive(const tinygltf::Primitive& gltfPrimitive)
    {
        MeshData mesh;

        switch (gltfPrimitive.mode)
        {
            case TINYGLTF_MODE_TRIANGLES:
            {
                mesh.PrimitiveTopology = PrimitiveTopology::TriangleList;
                break;
            }
            case TINYGLTF_MODE_TRIANGLE_STRIP:
            {
                mesh.PrimitiveTopology = PrimitiveTopology::TriangleStrip;
                break;
            }
            default:
            {
                BenzinEnsure(false);
                break;
            }
        }

        const int positionAccessorIndex = gltfPrimitive.attributes.contains("POSITION") ? gltfPrimitive.attributes.at("POSITION") : -1;
        const int normalAccessorIndex = gltfPrimitive.attributes.contains("NORMAL") ? gltfPrimitive.attributes.at("NORMAL") : -1;
        const int uvAccessorIndex = gltfPrimitive.attributes.contains("TEXCOORD_0") ? gltfPrimitive.attributes.at("TEXCOORD_0") : -1;
        const int indexAccessorIndex = gltfPrimitive.indices;
        BenzinEnsure(!gltfPrimitive.attributes.contains("TEXCOORD_1")); // #TODO

        const std::span positions = ParseGltfAccessor<DirectX::XMFLOAT3>(positionAccessorIndex);
        const std::span normals = ParseGltfAccessor<DirectX::XMFLOAT3>(normalAccessorIndex);
        const std::span uvs = ParseGltfAccessor<DirectX::XMFLOAT2>(uvAccessorIndex);
        const std::span indices = ParseGltfAccessor<IndexType>(indexAccessorIndex);

        BenzinEnsure(!positions.empty());

#if BENZIN_IS_ASSERTS_ENABLED
        if (!normals.empty())
        {
            BenzinAssert(normals.size() == positions.size());
        }

        if (!uvs.empty())
        {
            BenzinAssert(uvs.size() == positions.size());
        }
#endif

        // Fill vertices
        mesh.Vertices.resize(positions.size());
        for (const auto& [i, meshVertex] : mesh.Vertices | std::views::enumerate)
        {
            meshVertex.Position = positions[i];

            if (!normals.empty())
            {
                meshVertex.Normal = normals[i];
            }

            if (!uvs.empty())
            {
                meshVertex.Uv = uvs[i];
            }
        }

        // Fill indices
        mesh.Indices.resize(indices.size());
        if constexpr (std::is_same_v<IndexType, uint32_t>)
        {
            memcpy(mesh.Indices.data(), indices.data(), indices.size());
        }
        else
        {
            std::ranges::copy(indices, mesh.Indices.begin());
        }

        mesh.BoundingBox = ComputeBoundingBox(mesh.Vertices);

        return mesh;
    }

    void GltfReader::ParseGltfMesh(const tinygltf::Mesh& gltfMesh)
    {
        for (const tinygltf::Primitive& gltfPrimitive : gltfMesh.primitives)
        {
            BenzinAssert(gltfPrimitive.indices != -1);
            const tinygltf::Accessor& indexBufferAccessor = m_GltfModel->accessors[gltfPrimitive.indices];

            m_OutMesh->SubMeshes.push_back([this, &gltfPrimitive, &indexBufferAccessor]
            {
                switch (indexBufferAccessor.componentType)
                {
                    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE: return ParseGltfPrimitive<uint8_t>(gltfPrimitive);
                    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT: return ParseGltfPrimitive<uint16_t>(gltfPrimitive);
                    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT: return ParseGltfPrimitive<uint32_t>(gltfPrimitive);
                }

                BenzinEnsure(false);
                return MeshData{};
            }());
        }
    }

    void GltfReader::ParseGltfMeshes()
    {
        BenzinAssert(m_OutMesh->SubMeshes.empty());
        m_OutMesh->SubMeshes.reserve(std::ranges::fold_left(m_GltfModel->meshes, 0, [](size_t sum, const tinygltf::Mesh& gltfMesh)
        {
            return sum + gltfMesh.primitives.size();
        }));

        for (const tinygltf::Mesh& gltfMesh : m_GltfModel->meshes)
        {
            ParseGltfMesh(gltfMesh);
        }
    }

    void GltfReader::ParseGltfNode(int gltfNodeIndex, const DirectX::XMMATRIX& parentNodeTransform)
    {
        const tinygltf::Node& gltfNode = m_GltfModel->nodes[gltfNodeIndex];
        const DirectX::XMMATRIX nodeTransform = ParseNodeTransform(gltfNode, parentNodeTransform);

        if (const int gltfMeshIndex = gltfNode.mesh; gltfMeshIndex != -1)
        {
            for (const auto& [primitiveIndex, gltfPrimitive] : m_GltfModel->meshes[gltfMeshIndex].primitives | std::views::enumerate)
            {
                BenzinAssert(gltfPrimitive.material != -1);

                m_OutMesh->SubMeshInstances.push_back(joint::MeshInstance
                {
                    .SubMeshIndex = (uint32_t)(gltfMeshIndex + primitiveIndex),
                    .MaterialIndex = (uint32_t)gltfPrimitive.material,
                    .Transform = nodeTransform,
                });
            }
        }

        for (const int gltfChildNodeIndex : gltfNode.children)
        {
            ParseGltfNode(gltfChildNodeIndex, nodeTransform);
        }
    }

    void GltfReader::ParseGltfNodes()
    {
        // Convert from right-handed to left-handed
        // Must be used with TriangleOrder::CounterClockwise in rasterizer state
        const DirectX::XMMATRIX parentNodeTransform = DirectX::XMMatrixScaling(1.0f, 1.0f, -1.0f);

        for (const tinygltf::Scene& gltfScene : m_GltfModel->scenes)
        {
            for (const int gltfNodeIndex : gltfScene.nodes)
            {
                ParseGltfNode(gltfNodeIndex, parentNodeTransform);
            }
        }

        m_OutMesh->IsIndexOrderClockwise = false;
    }

    void GltfReader::ParseGltfMaterials()
    {
        BenzinAssert(m_OutMesh->Materials.empty());
        m_OutMesh->Materials.reserve(m_GltfModel->materials.size());

        for (const tinygltf::Material& gltfMaterial : m_GltfModel->materials)
        {
            const tinygltf::PbrMetallicRoughness& gltfPbrMetallicRoughness = gltfMaterial.pbrMetallicRoughness;

            Material& material = m_OutMesh->Materials.emplace_back();

            // Albedo
            {
                material.AlbedoTextureIndex = AddTextureMapping(gltfPbrMetallicRoughness.baseColorTexture.index, true);

                BenzinAssert(gltfPbrMetallicRoughness.baseColorFactor.size() == 4);
                material.AlbedoFactor.x = (float)gltfPbrMetallicRoughness.baseColorFactor[0];
                material.AlbedoFactor.y = (float)gltfPbrMetallicRoughness.baseColorFactor[1];
                material.AlbedoFactor.z = (float)gltfPbrMetallicRoughness.baseColorFactor[2];
                material.AlbedoFactor.w = (float)gltfPbrMetallicRoughness.baseColorFactor[3];

                material.AlphaCutoff = (float)gltfMaterial.alphaCutoff;
            }

            // Normal
            {
                material.NormalTextureIndex = AddTextureMapping(gltfMaterial.normalTexture.index, false);
                material.NormalScale = (float)gltfMaterial.normalTexture.scale;
            }

            // MetalRoughness
            {
                material.MetallicRoughnessTextureIndex = AddTextureMapping(gltfPbrMetallicRoughness.metallicRoughnessTexture.index, false);
                material.MetalnessFactor = (float)gltfPbrMetallicRoughness.metallicFactor;
                material.RoughnessFactor = (float)gltfPbrMetallicRoughness.roughnessFactor;
            }

            // Emissive
            {
                material.EmissiveTextureIndex = AddTextureMapping(gltfMaterial.emissiveTexture.index, true);

                BenzinAssert(gltfMaterial.emissiveFactor.size() == 3);
                material.EmissiveFactor.x = (float)gltfMaterial.emissiveFactor[0];
                material.EmissiveFactor.y = (float)gltfMaterial.emissiveFactor[1];
                material.EmissiveFactor.z = (float)gltfMaterial.emissiveFactor[2];
            }

            if (gltfMaterial.alphaMode == "MASK")
            {
                material.IsAlphaTestRequired = true;
            }
            else
            {
                BenzinAssert(gltfMaterial.alphaMode == "OPAQUE");
                material.IsAlphaTestRequired = false;
            }
        }
    }

    void GltfReader::ParseGltfTextures()
    {
        BenzinAssert(m_OutMesh->TextureImages.empty());
        m_OutMesh->TextureImages.resize(m_TextureMappings.size());

        std::for_each(std::execution::par, m_TextureMappings.begin(), m_TextureMappings.end(), [this](const auto textureMappingEntry)
        {
            const uint32_t gltfTextureIndex = textureMappingEntry.first;
            const TextureMapping textureMapping = textureMappingEntry.second;

            const tinygltf::Texture gltfTexture = m_GltfModel->textures[gltfTextureIndex];
            const tinygltf::Image& gltfImage = m_GltfModel->images[gltfTexture.source];
            BenzinAssert(gltfImage.bits == 8);
            BenzinAssert(gltfImage.pixel_type == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE);

            TextureImage textureImage
            {
                .Format = textureMapping.IsSrgb ? GraphicsFormat::Rgba8Unorm_Srgb : GraphicsFormat::Rgba8Unorm,
                .Width = (uint32_t)gltfImage.width,
                .Height = (uint32_t)gltfImage.height,
            };

            if (!gltfTexture.name.empty())
            {
                textureImage.DebugName = gltfTexture.name;
            }
            else if (!gltfImage.name.empty())
            {
                textureImage.DebugName = gltfImage.name;
            }
            else if (!gltfImage.uri.empty())
            {
                textureImage.DebugName = gltfImage.uri;
            }

            // Fill image
            textureImage.ImageData.resize(gltfImage.image.size());
            memcpy(textureImage.ImageData.data(), gltfImage.image.data(), gltfImage.image.size());

            m_OutMesh->TextureImages[textureMapping.MappedIndex] = std::move(textureImage);
        });
    }

    uint32_t GltfReader::AddTextureMapping(int gltfTextureIndex, bool isSrgb)
    {
        if (gltfTextureIndex == -1)
        {
            return g_Bad32;
        }

        if (!m_TextureMappings.contains(gltfTextureIndex))
        {
            const auto mappedIndex = (uint32_t)m_TextureMappings.size();

            auto& textureMapping = m_TextureMappings[gltfTextureIndex];
            textureMapping.MappedIndex = mappedIndex;
            textureMapping.IsSrgb = isSrgb;
        }

        return m_TextureMappings[gltfTextureIndex].MappedIndex;
    }

    void GltfReader::ResetState()
    {
        MakeUniquePtr(m_GltfModel);
        m_TextureMappings.clear();
        m_OutMesh = nullptr;
    }

}
