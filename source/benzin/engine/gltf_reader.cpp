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
    }

    GltfReader::~GltfReader() = default;

    bool GltfReader::ReadFromFile(
        std::string_view fileName,
        MeshResource& mesh,
        std::vector<MaterialResource>& materials,
        std::vector<TextureImage>& textures)
    {
        MakeUniquePtr(m_GltfModel);
        m_TextureMappings.clear();

        const std::filesystem::path filePath = EngineConfig::GetModelDir() / fileName;
        BenzinAssert(std::filesystem::exists(filePath));
        BenzinAssert(filePath.extension() == ".glb" || filePath.extension() == ".gltf");

        std::string error;
        std::string warning;
        bool isOk = false;

        const std::string filePathStr = filePath.string();

        {
            BenzinLogTimeOnScopeExit("GLTF Reader: LoadFromFile {}", filePathStr);

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

        ParseGltfMeshes(mesh);
        ParseGltfNodes(mesh);
        ParseGltfMaterials(materials);
        ParseGltfTextures(textures);

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
    void GltfReader::ParseGltfPrimitive(const tinygltf::Primitive& gltfPrimitive, MeshResource& mesh)
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

        MeshDrawRange drawRange;
        drawRange.m_VertexRange.m_Offset = (uint32_t)mesh.m_Vertices.size();
        drawRange.m_VertexRange.m_Count = (uint32_t)positions.size();
        drawRange.m_IndexRange.m_Offset = (uint32_t)mesh.m_Indices.size();
        drawRange.m_IndexRange.m_Count = (uint32_t)indices.size();
        drawRange.m_Topology = [&gltfPrimitive]
        {
            switch (gltfPrimitive.mode)
            {
            case TINYGLTF_MODE_TRIANGLES:
                return PrimitiveTopology::TriangleList;
            case TINYGLTF_MODE_TRIANGLE_STRIP:
                return PrimitiveTopology::TriangleStrip;
            }

            BenzinEnsure(false, "Unsupported primitive topology type: {}", gltfPrimitive.mode);
            return PrimitiveTopology::Unknown;
        }();

        mesh.m_DrawRanges.push_back(drawRange);
        mesh.m_Vertices.reserve(mesh.m_Vertices.size() + positions.size());
        mesh.m_Indices.reserve(mesh.m_Indices.size() + indices.size());

        for (uint32_t i = 0; i < positions.size(); ++i)
        {
            joint::MeshVertex& vertex = mesh.m_Vertices.emplace_back();

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

        BenzinAssert(indices.size() % 3 == 0);
        for (uint32_t i = 0; i < indices.size(); i += 3)
        {
            mesh.m_Indices.push_back(indices[i]);
            mesh.m_Indices.push_back(indices[i + 2]);
            mesh.m_Indices.push_back(indices[i + 1]);
        }
    }

    void GltfReader::ParseGltfMeshes(MeshResource& mesh)
    {
        BenzinAssert(mesh.m_DrawRanges.empty());

        uint32_t drawRangeCount = 0;
        for (const tinygltf::Mesh& gltfMesh : m_GltfModel->meshes)
        {
            drawRangeCount += (uint32_t)gltfMesh.primitives.size();
        }

        mesh.m_DrawRanges.reserve(drawRangeCount);

        for (const tinygltf::Mesh& gltfMesh : m_GltfModel->meshes)
        {
            for (const tinygltf::Primitive& gltfPrimitive : gltfMesh.primitives)
            {
                BenzinAssert(gltfPrimitive.indices != -1);
                const tinygltf::Accessor& indexBufferAccessor = m_GltfModel->accessors[gltfPrimitive.indices];

                switch (indexBufferAccessor.componentType)
                {
                case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
                    ParseGltfPrimitive<uint8_t>(gltfPrimitive, mesh);
                    break;
                case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
                    ParseGltfPrimitive<uint16_t>(gltfPrimitive, mesh);
                    break;
                case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
                    ParseGltfPrimitive<uint32_t>(gltfPrimitive, mesh);
                    break;
                default:
                    BenzinAssert(false, "Unsupported index buffer component type: {}", indexBufferAccessor.componentType);
                }
            }
        }
    }

    void GltfReader::ParseGltfNode(int gltfNodeIndex, const DirectX::XMMATRIX& parentObjectToLocal, MeshResource& mesh)
    {
        const tinygltf::Node& gltfNode = m_GltfModel->nodes[gltfNodeIndex];
        const DirectX::XMMATRIX objectToLocal = CalcObjectToLocalMatrix(gltfNode, parentObjectToLocal);

        const int gltfMeshIndex = gltfNode.mesh;
        if (gltfMeshIndex != -1)
        {
            for (uint32_t primitiveIndex = 0; primitiveIndex < m_GltfModel->meshes[gltfMeshIndex].primitives.size(); ++primitiveIndex)
            {

                MeshInstance instance;
                instance.m_ObjectToLocalMatrix = objectToLocal;
                instance.m_DrawRangeIndex = (uint32_t)(gltfMeshIndex + primitiveIndex);

                const int gltfMatrialIndex = m_GltfModel->meshes[gltfMeshIndex].primitives[primitiveIndex].material;
                if (gltfMatrialIndex != -1)
                {
                    instance.m_MaterialIndex = (uint32_t)gltfMatrialIndex;
                }

                mesh.m_Instances.push_back(instance);
            }
        }

        for (const int gltfChildNodeIndex : gltfNode.children)
        {
            ParseGltfNode(gltfChildNodeIndex, objectToLocal, mesh);
        }
    }

    void GltfReader::ParseGltfNodes(MeshResource& mesh)
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
                ParseGltfNode(gltfNodeIndex, parentObjectToLocal, mesh);
            }
        }
    }

    void GltfReader::ParseGltfMaterials(std::vector<MaterialResource>& materials)
    {
        BenzinAssert(materials.empty());
        materials.reserve(m_GltfModel->materials.size());

        for (const tinygltf::Material& gltfMaterial : m_GltfModel->materials)
        {
            const tinygltf::PbrMetallicRoughness& gltfPbrMetallicRoughness = gltfMaterial.pbrMetallicRoughness;

            MaterialResource& material = materials.emplace_back();

            // Albedo
            {
                material.m_TextureIndices.m_Albedo = AddTextureMapping(gltfPbrMetallicRoughness.baseColorTexture.index, true);

                BenzinAssert(gltfPbrMetallicRoughness.baseColorFactor.size() == 4);
                material.m_Consts.m_AlbedoFactor.x = (float)gltfPbrMetallicRoughness.baseColorFactor[0];
                material.m_Consts.m_AlbedoFactor.y = (float)gltfPbrMetallicRoughness.baseColorFactor[1];
                material.m_Consts.m_AlbedoFactor.z = (float)gltfPbrMetallicRoughness.baseColorFactor[2];
                material.m_Consts.m_AlbedoFactor.w = (float)gltfPbrMetallicRoughness.baseColorFactor[3];

                material.m_Consts.m_AlphaCutoff = (float)gltfMaterial.alphaCutoff;
            }

            // Normal
            {
                material.m_TextureIndices.m_Normal = AddTextureMapping(gltfMaterial.normalTexture.index, false);
                material.m_Consts.m_NormalScale = (float)gltfMaterial.normalTexture.scale;
            }

            // MetalRoughness
            {
                material.m_TextureIndices.m_MetallicRoughness = AddTextureMapping(gltfPbrMetallicRoughness.metallicRoughnessTexture.index, false);
                material.m_Consts.m_MetalnessFactor = (float)gltfPbrMetallicRoughness.metallicFactor;
                material.m_Consts.m_RoughnessFactor = (float)gltfPbrMetallicRoughness.roughnessFactor;
            }

            // Emissive
            {
                material.m_TextureIndices.m_Emissive = AddTextureMapping(gltfMaterial.emissiveTexture.index, true);

                BenzinAssert(gltfMaterial.emissiveFactor.size() == 3);
                material.m_Consts.m_EmissiveFactor.x = (float)gltfMaterial.emissiveFactor[0];
                material.m_Consts.m_EmissiveFactor.y = (float)gltfMaterial.emissiveFactor[1];
                material.m_Consts.m_EmissiveFactor.z = (float)gltfMaterial.emissiveFactor[2];
            }

            if (gltfMaterial.alphaMode == "MASK")
            {
                material.m_Consts.m_IsAlphaTestRequired = true;
            }
            else
            {
                BenzinAssert(gltfMaterial.alphaMode == "OPAQUE");
                material.m_Consts.m_IsAlphaTestRequired = false;
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
            textureImage.m_Format = textureMapping.m_IsSrgb ? GraphicsFormat::Rgba8Unorm_Srgb : GraphicsFormat::Rgba8Unorm;
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
            return g_Bad32;

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
