#include <benzin/config/bootstrap.hpp>
#include <benzin/engine/gltf_reader.hpp>

#include <stb_image.h>
#include <tiny_gltf.h>

#include <shaders/joint/mesh_types.hpp>

#include <benzin/core/engine_math.hpp>
#include <benzin/engine/mesh.hpp>

namespace benzin
{

    static DirectX::XMMATRIX FlipZHandedness(const DirectX::XMMATRIX& rightHandedMatrix)
    {
        static const DirectX::XMMATRIX flipZ = DirectX::XMMatrixScaling(1.0f, 1.0f, -1.0f);

        return flipZ * rightHandedMatrix * flipZ; // Apply from both sides to flip handedness without flipping position
    }

    static DirectX::XMMATRIX CalcObjectToLocalMatrix(const tinygltf::Node& gltfNode, const DirectX::XMMATRIX& parentObjectToLocal)
    {
        DirectX::XMMATRIX objectToLocal = DirectX::XMMatrixIdentity();

        if (!gltfNode.matrix.empty())
        {
            // Actually 'gltfNode.matrix' is already in row-major order. So there is no need to transpose it
            // Ref: glTF docs: https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#transformations
            // Ref: Row-major vs column-major matrices: https://gamedev.stackexchange.com/questions/153816/why-do-these-directxmath-functions-seem-like-they-return-column-major-matrics

            BenzinAssert(gltfNode.matrix.size() == 16);

            for (uint32_t i = 0; i < 4; ++i)
            {
                objectToLocal.r[i] = DirectX::XMVECTOR
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

                objectToLocal *= DirectX::XMMatrixRotationQuaternion(DirectX::XMLoadFloat4(&rotation));
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

                objectToLocal *= DirectX::XMMatrixScalingFromVector(DirectX::XMLoadFloat3(&scale));
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

                objectToLocal *= DirectX::XMMatrixTranslationFromVector(DirectX::XMLoadFloat3(&translation));
            }
        }

        return FlipZHandedness(objectToLocal) * parentObjectToLocal;
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

        const uint64_t dataOffsetInBytes = gltfBufferView.byteOffset + gltfAccessor.byteOffset;

        return std::span
        {
            reinterpret_cast<const T*>(gltfBuffer.data.data() + dataOffsetInBytes),
            gltfAccessor.count
        };
    }

    template <std::integral IndexType>
    void GltfReader::ParseGltfPrimitive(const tinygltf::Primitive& gltfPrimitive)
    {
        const int positionAccessorIndex = gltfPrimitive.attributes.contains("POSITION") ? gltfPrimitive.attributes.at("POSITION") : -1;
        const int normalAccessorIndex = gltfPrimitive.attributes.contains("NORMAL") ? gltfPrimitive.attributes.at("NORMAL") : -1;
        const int uvAccessorIndex = gltfPrimitive.attributes.contains("TEXCOORD_0") ? gltfPrimitive.attributes.at("TEXCOORD_0") : -1;
        const int indexAccessorIndex = gltfPrimitive.indices;
        BenzinEnsure(!gltfPrimitive.attributes.contains("TEXCOORD_1"));

        const std::span positions = ParseGltfAccessor<DirectX::XMFLOAT3>(positionAccessorIndex);
        const std::span normals = ParseGltfAccessor<DirectX::XMFLOAT3>(normalAccessorIndex);
        const std::span uvs = ParseGltfAccessor<DirectX::XMFLOAT2>(uvAccessorIndex);
        const std::span indices = ParseGltfAccessor<IndexType>(indexAccessorIndex);

        BenzinEnsure(!positions.empty());
        BenzinEnsure(normals.empty() || normals.size() == positions.size());
        BenzinEnsure(uvs.empty() || uvs.size() == uvs.size());

        const auto vertexCount = (uint32_t)positions.size();

        {
            MeshDrawRange drawRange
            {
                .VertexRange{ (uint32_t)m_OutMesh->Vertices.size(), vertexCount },
                .IndexRange{ (uint32_t)m_OutMesh->Indices.size(), (uint32_t)indices.size() },

                .PrimitiveTopology = [&gltfPrimitive]
                {
                    switch (gltfPrimitive.mode)
                    {
                        case TINYGLTF_MODE_TRIANGLES: return PrimitiveTopology::TriangleList;
                        case TINYGLTF_MODE_TRIANGLE_STRIP: return PrimitiveTopology::TriangleStrip;
                    }

                    BenzinEnsure(false, "Unsupported primitive topology type: {}", gltfPrimitive.mode);
                    return PrimitiveTopology::Unknown;
                }(),
            };

            m_OutMesh->DrawRanges.push_back(drawRange);
        }

        m_OutMesh->Vertices.reserve(m_OutMesh->Vertices.size() + vertexCount);
        m_OutMesh->Indices.reserve(m_OutMesh->Indices.size() + indices.size());

        for (uint32_t i = 0; i < vertexCount; ++i)
        {
            joint::MeshVertex& vertex = m_OutMesh->Vertices.emplace_back();

            vertex.Position = positions[i];
            vertex.Position.z = -vertex.Position.z;
            
            if (!normals.empty())
            {
                vertex.Normal = normals[i];
                vertex.Normal.z = -vertex.Normal.z;
            }

            if (!uvs.empty())
            {
                vertex.Uv = uvs[i];
            }
        }

        m_OutMesh->Indices.reserve(m_OutMesh->Indices.size() + indices.size());

        BenzinAssert(indices.size() % 3 == 0);
        for (uint32_t i = 0; i < indices.size(); i += 3)
        {
            m_OutMesh->Indices.push_back(indices[i]);
            m_OutMesh->Indices.push_back(indices[i + 2]);
            m_OutMesh->Indices.push_back(indices[i + 1]);
        }
    }

    void GltfReader::ParseGltfMesh(const tinygltf::Mesh& gltfMesh)
    {
        for (const tinygltf::Primitive& gltfPrimitive : gltfMesh.primitives)
        {
            BenzinAssert(gltfPrimitive.indices != -1);
            const tinygltf::Accessor& indexBufferAccessor = m_GltfModel->accessors[gltfPrimitive.indices];

            switch (indexBufferAccessor.componentType)
            {
                case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE: ParseGltfPrimitive<uint8_t>(gltfPrimitive); continue;
                case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT: ParseGltfPrimitive<uint16_t>(gltfPrimitive); continue;
                case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT: ParseGltfPrimitive<uint32_t>(gltfPrimitive); continue;
            }

            BenzinAssert(false, "Unsupported index buffer component type: {}", indexBufferAccessor.componentType);
        }
    }

    void GltfReader::ParseGltfMeshes()
    {
        BenzinAssert(m_OutMesh->DrawRanges.empty());

        uint32_t drawRangeCount = 0;
        for (const tinygltf::Mesh gltfMesh : m_GltfModel->meshes)
        {
            drawRangeCount += (uint32_t)gltfMesh.primitives.size();
        }

        m_OutMesh->DrawRanges.reserve(drawRangeCount);

        for (const tinygltf::Mesh& gltfMesh : m_GltfModel->meshes)
        {
            ParseGltfMesh(gltfMesh);
        }
    }

    void GltfReader::ParseGltfNode(int gltfNodeIndex, const DirectX::XMMATRIX& parentObjectToLocal)
    {
        const tinygltf::Node& gltfNode = m_GltfModel->nodes[gltfNodeIndex];
        const DirectX::XMMATRIX objectToLocal = CalcObjectToLocalMatrix(gltfNode, parentObjectToLocal);

        if (const int gltfMeshIndex = gltfNode.mesh; gltfMeshIndex != -1)
        {
            for (const auto& [primitiveIndex, gltfPrimitive] : m_GltfModel->meshes[gltfMeshIndex].primitives | std::views::enumerate)
            {
                BenzinAssert(gltfPrimitive.material != -1);

                m_OutMesh->Instances.push_back(MeshInstance
                {
                    .ObjectToLocalMatrix = objectToLocal,
                    .DrawRangeIndex = (uint32_t)(gltfMeshIndex + primitiveIndex),
                    .MaterialIndex = (uint32_t)gltfPrimitive.material,
                });
            }
        }

        for (const int gltfChildNodeIndex : gltfNode.children)
        {
            ParseGltfNode(gltfChildNodeIndex, objectToLocal);
        }
    }

    void GltfReader::ParseGltfNodes()
    {
        // NOTE: GLTF meshes use right-handed (RH) system.
        // So during parsing there are key steps which are mandatory to use GLTF meshes with LH matrices:
        //   1. Flip positions and normals in z coordinate
        //   2. Flip triangle order
        //   3. Convert node transform from RH to LH

        const DirectX::XMMATRIX parentObjectToLocal = DirectX::XMMatrixIdentity();

        for (const tinygltf::Scene& gltfScene : m_GltfModel->scenes)
        {
            for (const int gltfNodeIndex : gltfScene.nodes)
            {
                ParseGltfNode(gltfNodeIndex, parentObjectToLocal);
            }
        }
    }

    void GltfReader::ParseGltfMaterials()
    {
        BenzinAssert(m_OutMesh->Materials.empty());
        m_OutMesh->Materials.reserve(m_GltfModel->materials.size());

        for (const tinygltf::Material& gltfMaterial : m_GltfModel->materials)
        {
            const tinygltf::PbrMetallicRoughness& gltfPbrMetallicRoughness = gltfMaterial.pbrMetallicRoughness;

            MeshResource::Material& material = m_OutMesh->Materials.emplace_back();

            // Albedo
            {
                material.TextureIndices.Albedo = AddTextureMapping(gltfPbrMetallicRoughness.baseColorTexture.index, true);

                BenzinAssert(gltfPbrMetallicRoughness.baseColorFactor.size() == 4);
                material.Consts.AlbedoFactor.x = (float)gltfPbrMetallicRoughness.baseColorFactor[0];
                material.Consts.AlbedoFactor.y = (float)gltfPbrMetallicRoughness.baseColorFactor[1];
                material.Consts.AlbedoFactor.z = (float)gltfPbrMetallicRoughness.baseColorFactor[2];
                material.Consts.AlbedoFactor.w = (float)gltfPbrMetallicRoughness.baseColorFactor[3];

                material.Consts.AlphaCutoff = (float)gltfMaterial.alphaCutoff;
            }

            // Normal
            {
                material.TextureIndices.Normal = AddTextureMapping(gltfMaterial.normalTexture.index, false);

                material.Consts.NormalScale = (float)gltfMaterial.normalTexture.scale;
            }

            // MetalRoughness
            {
                material.TextureIndices.MetallicRoughness = AddTextureMapping(gltfPbrMetallicRoughness.metallicRoughnessTexture.index, false);

                material.Consts.MetalnessFactor = (float)gltfPbrMetallicRoughness.metallicFactor;
                material.Consts.RoughnessFactor = (float)gltfPbrMetallicRoughness.roughnessFactor;
            }

            // Emissive
            {
                material.TextureIndices.Emissive = AddTextureMapping(gltfMaterial.emissiveTexture.index, true);

                BenzinAssert(gltfMaterial.emissiveFactor.size() == 3);
                material.Consts.EmissiveFactor.x = (float)gltfMaterial.emissiveFactor[0];
                material.Consts.EmissiveFactor.y = (float)gltfMaterial.emissiveFactor[1];
                material.Consts.EmissiveFactor.z = (float)gltfMaterial.emissiveFactor[2];
            }

            if (gltfMaterial.alphaMode == "MASK")
            {
                material.Consts.IsAlphaTestRequired = true;
            }
            else
            {
                BenzinAssert(gltfMaterial.alphaMode == "OPAQUE");
                material.Consts.IsAlphaTestRequired = false;
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

            const tinygltf::Texture& gltfTexture = m_GltfModel->textures[gltfTextureIndex];
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
            textureImage.PixelData.resize(gltfImage.image.size());
            memcpy(textureImage.PixelData.data(), gltfImage.image.data(), gltfImage.image.size());

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
