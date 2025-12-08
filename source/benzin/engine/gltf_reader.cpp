#include <benzin/config/bootstrap.hpp>
#include <benzin/engine/gltf_reader.hpp>

#include <stb_image.h>
#include <tiny_gltf.h>

#include <shaders/joint/mesh_types.hpp>

#include <benzin/engine/mesh.hpp>

namespace benzin
{

    static DirectX::XMMATRIX FlipZHandedness(const DirectX::XMMATRIX& rightHandedMatrix)
    {
        const DirectX::XMMATRIX flipZ = DirectX::XMMatrixScaling(1.0f, 1.0f, -1.0f);

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

                DirectX::XMFLOAT4 rotation;
                rotation.x = (float)gltfNode.rotation[0];
                rotation.y = (float)gltfNode.rotation[1];
                rotation.z = (float)gltfNode.rotation[2];
                rotation.w = (float)gltfNode.rotation[3];

                objectToLocal *= DirectX::XMMatrixRotationQuaternion(DirectX::XMLoadFloat4(&rotation));
            }

            if (!gltfNode.scale.empty())
            {
                BenzinAssert(gltfNode.scale.size() == 3);

                DirectX::XMFLOAT3 scale;
                scale.x = (float)gltfNode.scale[0];
                scale.y = (float)gltfNode.scale[1];
                scale.z = (float)gltfNode.scale[2];

                objectToLocal *= DirectX::XMMatrixScalingFromVector(DirectX::XMLoadFloat3(&scale));
            }

            if (!gltfNode.translation.empty())
            {
                BenzinAssert(gltfNode.translation.size() == 3);

                DirectX::XMFLOAT3 translation;
                translation.x = (float)gltfNode.translation[0];
                translation.y = (float)gltfNode.translation[1];
                translation.z = (float)gltfNode.translation[2];

                objectToLocal *= DirectX::XMMatrixTranslationFromVector(DirectX::XMLoadFloat3(&translation));
            }
        }

        return FlipZHandedness(objectToLocal) * parentObjectToLocal;
    }

    //

    GltfReader::GltfReader()
    {
        MakeUniquePtr(m_GltfContext);
    }

    GltfReader::~GltfReader() = default;

    bool GltfReader::ReadFromFile(
        std::string_view fileName,
        MeshGeometry& geometry,
        std::vector<MeshDraw>& meshDraws,
        std::vector<Material>& materials,
        std::vector<TextureImage>& textures)
    {
        MakeUniquePtr(m_GltfModel);

        const std::filesystem::path filePath = GetModelDir() / fileName;
        BenzinAssert(std::filesystem::exists(filePath));
        BenzinAssert(filePath.extension() == ".glb" || filePath.extension() == ".gltf");

        std::string error;
        std::string warning;
        bool isOk = false;

        const std::string filePathStr = filePath.string();

        {
            BenzinTraceScopeTime("GLTF Reader: LoadFromFile {}", filePathStr);

            if (filePath.extension() == ".glb")
            {
                isOk = m_GltfContext->LoadBinaryFromFile(m_GltfModel.get(), &error, &warning, filePathStr);
            }
            else if (filePath.extension() == ".gltf")
            {
                isOk = m_GltfContext->LoadASCIIFromFile(m_GltfModel.get(), &error, &warning, filePathStr);
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
        else if (!isOk)
        {
            BenzinWarning("GLTF Reader: Failed to load from {}. There is no specific error", filePathStr);
            return false;
        }

        ParseGltfMeshes(geometry);
        ParseGltfNodes(meshDraws);
        ParseGltfMaterials(materials);
        ParseGltfTextures(textures);

        m_GltfModel.reset();
        m_TextureMappings.clear();

        return true;
    }

    template <typename T>
    std::span<const T> GltfReader::ParseGltfAccessor(int gltfAccessorIndex)
    {
        if (gltfAccessorIndex == -1)
            return {};

        const tinygltf::Accessor& gltfAccessor = m_GltfModel->accessors[gltfAccessorIndex];
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
    void GltfReader::ParseGltfPrimitive(const tinygltf::Primitive& gltfPrimitive, MeshGeometry& geometry)
    {
        BenzinEnsure(gltfPrimitive.mode == TINYGLTF_MODE_TRIANGLES);

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

        Mesh mesh;
        mesh.m_VertexOffset = (uint32_t)geometry.m_Vertices.size();
        mesh.m_VertexCount = (uint32_t)positions.size();
        mesh.m_Lods[0].m_IndexOffset = (uint32_t)geometry.m_Indices.size();
        mesh.m_Lods[0].m_IndexCount = (uint32_t)indices.size();
        mesh.m_LodCount = 1;

        geometry.m_Meshes.push_back(mesh);
        geometry.m_Vertices.reserve(geometry.m_Vertices.size() + positions.size());
        geometry.m_Indices.reserve(geometry.m_Indices.size() + indices.size());

        for (uint32_t i = 0; i < positions.size(); ++i)
        {
            joint::MeshVertex& vertex = geometry.m_Vertices.emplace_back();

            vertex.m_Position = positions[i];
            vertex.m_Position.z = -vertex.m_Position.z;
            
            if (!normals.empty())
            {
                vertex.m_Normal = normals[i];
                vertex.m_Normal.z = -vertex.m_Normal.z;
            }

            if (!uvs.empty())
            {
                vertex.m_Uv = uvs[i];
            }
        }

        BenzinAssert(indices.size() % 3 == 0);
        for (uint32_t i = 0; i < indices.size(); i += 3)
        {
            geometry.m_Indices.push_back(indices[i]);
            geometry.m_Indices.push_back(indices[i + 2]);
            geometry.m_Indices.push_back(indices[i + 1]);
        }
    }

    void GltfReader::ParseGltfMeshes(MeshGeometry& geometry)
    {
        size_t meshCount = 0;
        for (const tinygltf::Mesh& gltfMesh : m_GltfModel->meshes)
        {
            meshCount += gltfMesh.primitives.size();
        }

        BenzinAssert(geometry.m_Meshes.empty());
        geometry.m_Meshes.reserve(meshCount);

        for (const tinygltf::Mesh& gltfMesh : m_GltfModel->meshes)
        {
            for (const tinygltf::Primitive& gltfPrimitive : gltfMesh.primitives)
            {
                BenzinAssert(gltfPrimitive.indices != -1);
                const tinygltf::Accessor& indexBufferAccessor = m_GltfModel->accessors[gltfPrimitive.indices];

                switch (indexBufferAccessor.componentType)
                {
                    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
                        ParseGltfPrimitive<uint8_t>(gltfPrimitive, geometry);
                        break;
                    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
                        ParseGltfPrimitive<uint16_t>(gltfPrimitive, geometry);
                        break;
                    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
                        ParseGltfPrimitive<uint32_t>(gltfPrimitive, geometry);
                        break;
                    default:
                        BenzinAssert(false, "Unsupported index buffer component type: {}", indexBufferAccessor.componentType);
                }
            }
        }
    }

    void GltfReader::ParseGltfNode(int gltfNodeIndex, const DirectX::XMMATRIX& parentObjectToLocal, std::vector<MeshDraw>& meshDraws)
    {
        const tinygltf::Node& gltfNode = m_GltfModel->nodes[gltfNodeIndex];
        const DirectX::XMMATRIX objectToLocal = CalcObjectToLocalMatrix(gltfNode, parentObjectToLocal);

        const int gltfMeshIndex = gltfNode.mesh;
        if (gltfMeshIndex != -1)
        {
            for (uint32_t primitiveIndex = 0; primitiveIndex < m_GltfModel->meshes[gltfMeshIndex].primitives.size(); ++primitiveIndex)
            {
                MeshDraw& draw = meshDraws.emplace_back();
                draw.m_ObjectToLocal = objectToLocal;
                draw.m_MeshIndex = (uint32_t)(gltfMeshIndex + primitiveIndex);

                const int gltfMatrialIndex = m_GltfModel->meshes[gltfMeshIndex].primitives[primitiveIndex].material;
                if (gltfMatrialIndex != -1)
                {
                    draw.m_MaterialIndex = (uint32_t)gltfMatrialIndex;
                }
            }
        }

        for (const int gltfChildNodeIndex : gltfNode.children)
        {
            ParseGltfNode(gltfChildNodeIndex, objectToLocal, meshDraws);
        }
    }

    void GltfReader::ParseGltfNodes(std::vector<MeshDraw>& meshDraws)
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
                ParseGltfNode(gltfNodeIndex, parentObjectToLocal, meshDraws);
            }
        }
    }

    void GltfReader::ParseGltfMaterials(std::vector<Material>& materials)
    {
        BenzinAssert(materials.empty());
        materials.reserve(m_GltfModel->materials.size());

        for (const tinygltf::Material& gltfMaterial : m_GltfModel->materials)
        {
            const tinygltf::PbrMetallicRoughness& gltfPbrMetallicRoughness = gltfMaterial.pbrMetallicRoughness;

            Material& material = materials.emplace_back();

            // Albedo
            {
                BenzinAssert(gltfPbrMetallicRoughness.baseColorFactor.size() == 4);

                material.m_AlbedoTextureIndex = AddTextureMapping(gltfPbrMetallicRoughness.baseColorTexture.index, true);
                material.m_AlbedoFactor.x = (float)gltfPbrMetallicRoughness.baseColorFactor[0];
                material.m_AlbedoFactor.y = (float)gltfPbrMetallicRoughness.baseColorFactor[1];
                material.m_AlbedoFactor.z = (float)gltfPbrMetallicRoughness.baseColorFactor[2];
                material.m_AlbedoFactor.w = (float)gltfPbrMetallicRoughness.baseColorFactor[3];
                material.m_AlphaCutoff = (float)gltfMaterial.alphaCutoff;
            }

            // Normal
            {
                material.m_NormalTextureIndex = AddTextureMapping(gltfMaterial.normalTexture.index, false);
                material.m_NormalScale = (float)gltfMaterial.normalTexture.scale;
            }

            // MetalRoughness
            {
                material.m_MetallicRoughnessTextureIndex = AddTextureMapping(gltfPbrMetallicRoughness.metallicRoughnessTexture.index, false);
                material.m_MetalnessFactor = (float)gltfPbrMetallicRoughness.metallicFactor;
                material.m_RoughnessFactor = (float)gltfPbrMetallicRoughness.roughnessFactor;
            }

            // Emissive
            {
                BenzinAssert(gltfMaterial.emissiveFactor.size() == 3);

                material.m_EmissiveTextureIndex = AddTextureMapping(gltfMaterial.emissiveTexture.index, true);
                material.m_EmissiveFactor.x = (float)gltfMaterial.emissiveFactor[0];
                material.m_EmissiveFactor.y = (float)gltfMaterial.emissiveFactor[1];
                material.m_EmissiveFactor.z = (float)gltfMaterial.emissiveFactor[2];
            }

            if (gltfMaterial.alphaMode == "MASK")
            {
                material.m_IsAlphaTestRequired = true;
            }
            else
            {
                BenzinAssert(gltfMaterial.alphaMode == "OPAQUE");
                material.m_IsAlphaTestRequired = false;
            }
        }
    }

    void GltfReader::ParseGltfTextures(std::vector<TextureImage>& textures)
    {
        BenzinAssert(textures.empty());
        textures.resize(m_TextureMappings.size());

        std::for_each(std::execution::par, m_TextureMappings.begin(), m_TextureMappings.end(), [&](const auto textureMappingEntry)
        {
            const uint32_t gltfTextureIndex = textureMappingEntry.first;
            const TextureMapping textureMapping = textureMappingEntry.second;

            const tinygltf::Texture& gltfTexture = m_GltfModel->textures[gltfTextureIndex];
            const tinygltf::Image& gltfImage = m_GltfModel->images[gltfTexture.source];
            BenzinAssert(gltfImage.bits == 8);
            BenzinAssert(gltfImage.pixel_type == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE);

            TextureImage textureImage;
            textureImage.m_DxgiFormat = textureMapping.m_IsSrgb ? DXGI_FORMAT_R8G8B8A8_UNORM_SRGB : DXGI_FORMAT_R8G8B8A8_UNORM;
            textureImage.m_Width = (uint32_t)gltfImage.width;
            textureImage.m_Height = (uint32_t)gltfImage.height;

            if (!gltfTexture.name.empty())
            {
                textureImage.m_DebugName = gltfTexture.name;
            }
            else if (!gltfImage.name.empty())
            {
                textureImage.m_DebugName = gltfImage.name;
            }
            else if (!gltfImage.uri.empty())
            {
                textureImage.m_DebugName = gltfImage.uri;
            }

            // Fill image
            textureImage.m_PixelData.resize(gltfImage.image.size());
            memcpy(textureImage.m_PixelData.data(), gltfImage.image.data(), gltfImage.image.size());

            textures[textureMapping.m_MappedIndex] = std::move(textureImage);
        });
    }

    uint32_t GltfReader::AddTextureMapping(int gltfTextureIndex, bool isSrgb)
    {
        if (gltfTextureIndex == -1)
            return g_MaxU32;

        if (!m_TextureMappings.contains(gltfTextureIndex))
        {
            const auto mappedIndex = (uint32_t)m_TextureMappings.size();

            TextureMapping& textureMapping = m_TextureMappings[gltfTextureIndex];
            textureMapping.m_MappedIndex = mappedIndex;
            textureMapping.m_IsSrgb = isSrgb;
        }

        return m_TextureMappings[gltfTextureIndex].m_MappedIndex;
    }

}
