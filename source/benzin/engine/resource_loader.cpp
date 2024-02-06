#include "benzin/config/bootstrap.hpp"
#include "benzin/engine/resource_loader.hpp"

#include <third_party/tinygltf/stb_image.h>
#include <third_party/tinygltf/tiny_gltf.h>

namespace benzin
{

    namespace
    {
    
        class GLTFReader
        {
        public:
            bool ReadFromFile(std::string_view fileName, MeshCollectionResource& outMeshCollection)
            {
                BenzinLogTimeOnScopeExit("GLTF Reader: ReadFromFile");

                static tinygltf::TinyGLTF context;

                const std::filesystem::path filePath = config::g_ModelDirPath / fileName;
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
                        isFileLoadingSucceed = context.LoadBinaryFromFile(&m_CurrentModel, &error, &warning, filePathStr);
                    }
                    else if (filePath.extension() == ".gltf")
                    {
                        isFileLoadingSucceed = context.LoadASCIIFromFile(&m_CurrentModel, &error, &warning, filePathStr);
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

                outMeshCollection.DebugName = CutExtension(fileName);

                {
                    BenzinLogTimeOnScopeExit("GLTF Reader: {} ParseMeshPrimitives", outMeshCollection.DebugName);
                    ParseMeshPrimitives(outMeshCollection);
                }

                {
                    BenzinLogTimeOnScopeExit("GLTF Reader: {} ParseNodes", outMeshCollection.DebugName);
                    ParseNodes(outMeshCollection);
                }

                {
                    BenzinLogTimeOnScopeExit("GLTF Reader: {} ParseMaterials", outMeshCollection.DebugName);
                    ParseMaterials(outMeshCollection);
                }

                {
                    BenzinLogTimeOnScopeExit("GLTF Reader: {} ParseTextures", outMeshCollection.DebugName);
                    ParseTextures(outMeshCollection);
                }

                ResetState();

                return true;
            }

        private:
            template <typename T>
            std::span<const T> GetBufferFromGLTFAccessor(int accessorIndex)
            {
                if (accessorIndex == -1)
                {
                    return {};
                }

                const tinygltf::Accessor& gltfAccessor = m_CurrentModel.accessors[accessorIndex];
                const tinygltf::BufferView& gltfBufferView = m_CurrentModel.bufferViews[gltfAccessor.bufferView];
                const tinygltf::Buffer& gltfBuffer = m_CurrentModel.buffers[gltfBufferView.buffer];

                const size_t offsetInBytes = gltfBufferView.byteOffset + gltfAccessor.byteOffset;

                return std::span
                {
                    reinterpret_cast<const T*>(gltfBuffer.data.data() + offsetInBytes),
                    gltfAccessor.count
                };
            };

            template <std::integral IndexType>
            void ParseMeshPrimitive(const tinygltf::Primitive& gltfPrimitive, MeshCollectionResource& outMeshCollection)
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
                        BenzinAssert(false);
                        break;
                    }
                }

                const int positionAccessorIndex = gltfPrimitive.attributes.contains("POSITION") ? gltfPrimitive.attributes.at("POSITION") : -1;
                const int normalAccessorIndex = gltfPrimitive.attributes.contains("NORMAL") ? gltfPrimitive.attributes.at("NORMAL") : -1;
                const int uvAccessorIndex = gltfPrimitive.attributes.contains("TEXCOORD_0") ? gltfPrimitive.attributes.at("TEXCOORD_0") : -1;
                const int indexAccessorIndex = gltfPrimitive.indices;
                BenzinAssert(!gltfPrimitive.attributes.contains("TEXCOORD_1")); // #TODO

                const auto positions = GetBufferFromGLTFAccessor<DirectX::XMFLOAT3>(positionAccessorIndex);
                const auto normals = GetBufferFromGLTFAccessor<DirectX::XMFLOAT3>(normalAccessorIndex);
                const auto uvs = GetBufferFromGLTFAccessor<DirectX::XMFLOAT2>(uvAccessorIndex);
                const auto indices = GetBufferFromGLTFAccessor<IndexType>(indexAccessorIndex);

                BenzinAssert(!positions.empty());

                if (!normals.empty())
                {
                    BenzinAssert(normals.size() == positions.size());
                }

                if (!uvs.empty())
                {
                    BenzinAssert(uvs.size() == positions.size());
                }

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
                        meshVertex.UV = uvs[i];
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

                outMeshCollection.Meshes.push_back(std::move(mesh));
            }

            void ParseMesh(const tinygltf::Mesh& gltfMesh, MeshCollectionResource& outMeshCollection)
            {
                for (const tinygltf::Primitive& gltfPrimitive : gltfMesh.primitives)
                {
                    BenzinAssert(gltfPrimitive.indices != -1);
                    const tinygltf::Accessor& indexBufferAccessor = m_CurrentModel.accessors[gltfPrimitive.indices];

                    switch (indexBufferAccessor.componentType)
                    {
                        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
                        {
                            ParseMeshPrimitive<uint8_t>(gltfPrimitive, outMeshCollection);
                            break;
                        }
                        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
                        {
                            ParseMeshPrimitive<uint16_t>(gltfPrimitive, outMeshCollection);
                            break;
                        }
                        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
                        {
                            ParseMeshPrimitive<uint32_t>(gltfPrimitive, outMeshCollection);
                            break;
                        }
                        default:
                        {
                            BenzinAssert(false);
                            break;
                        }
                    }
                }
            }

            void ParseMeshPrimitives(MeshCollectionResource& outMeshCollection)
            {
                const size_t totalMeshCount = std::ranges::fold_left(m_CurrentModel.meshes, 0, [](size_t sum, const auto& gltfMesh) { return sum + gltfMesh.primitives.size(); });
                outMeshCollection.Meshes.reserve(totalMeshCount);

                for (const tinygltf::Mesh& gltfMesh : m_CurrentModel.meshes)
                {
                    ParseMesh(gltfMesh, outMeshCollection);
                }
            }

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

            void ParseNode(int gltfNodeIndex, int gltfParentNodeIndex, const DirectX::XMMATRIX& parentNodeTransform, MeshCollectionResource& outMeshCollection)
            {
                const tinygltf::Node& gltfNode = m_CurrentModel.nodes[gltfNodeIndex];
                const DirectX::XMMATRIX nodeTransform = ParseNodeTransform(gltfNode, parentNodeTransform);

                if (const int meshIndex = gltfNode.mesh; meshIndex != -1)
                {
                    const size_t currentNodeIndex = outMeshCollection.MeshNodes.size();
                    outMeshCollection.MeshNodes.push_back(MeshNode{ nodeTransform });

                    for (const auto& [primitiveIndex, gltfPrimitive] : m_CurrentModel.meshes[meshIndex].primitives | std::views::enumerate)
                    {
                        BenzinAssert(gltfPrimitive.material != -1);

                        outMeshCollection.MeshInstances.push_back(MeshInstance
                        {
                            .MeshIndex = (uint32_t)(meshIndex + primitiveIndex),
                            .MaterialIndex = (uint32_t)gltfPrimitive.material,
                            .MeshNodeIndex = (uint32_t)currentNodeIndex,
                        });
                    }
                }

                for (const int gltfChildNodeIndex : gltfNode.children)
                {
                    ParseNode(gltfChildNodeIndex, gltfNodeIndex, nodeTransform, outMeshCollection);
                }
            }

            void ParseNodes(MeshCollectionResource& outMeshCollection)
            {
                const int gltfParentNodeIndex = -1;

                // Convert from right-handed to left-handed
                // Must be used with TriangleOrder::CounterClockwise in rasterizer state
                const DirectX::XMMATRIX parentNodeTransform = DirectX::XMMatrixScaling(1.0f, 1.0f, -1.0f);

                for (const tinygltf::Scene& gltfScene : m_CurrentModel.scenes)
                {
                    for (const int gltfNodeIndex : gltfScene.nodes)
                    {
                        ParseNode(gltfNodeIndex, gltfParentNodeIndex, parentNodeTransform, outMeshCollection);
                    }
                }
            }

            void ParseMaterials(MeshCollectionResource& outMeshCollection)
            {
                outMeshCollection.Materials.reserve(m_CurrentModel.materials.size());

                for (const auto& gltfMaterial : m_CurrentModel.materials)
                {
                    const tinygltf::PbrMetallicRoughness& gltfPbrMetallicRoughness = gltfMaterial.pbrMetallicRoughness;

                    Material material;

                    // Albedo
                    {
                        material.AlbedoTextureIndex = PushTextureMapping(gltfPbrMetallicRoughness.baseColorTexture.index);

                        BenzinAssert(gltfPbrMetallicRoughness.baseColorFactor.size() == 4);
                        material.AlbedoFactor.x = (float)gltfPbrMetallicRoughness.baseColorFactor[0];
                        material.AlbedoFactor.y = (float)gltfPbrMetallicRoughness.baseColorFactor[1];
                        material.AlbedoFactor.z = (float)gltfPbrMetallicRoughness.baseColorFactor[2];
                        material.AlbedoFactor.w = (float)gltfPbrMetallicRoughness.baseColorFactor[3];

                        material.AlphaCutoff = (float)gltfMaterial.alphaCutoff;
                    }

                    // Normal
                    {
                        material.NormalTextureIndex = PushTextureMapping(gltfMaterial.normalTexture.index);
                        material.NormalScale = (float)gltfMaterial.normalTexture.scale;
                    }

                    // MetalRoughness
                    {
                        material.MetalRoughnessTextureIndex = PushTextureMapping(gltfPbrMetallicRoughness.metallicRoughnessTexture.index);
                        material.MetalnessFactor = (float)gltfPbrMetallicRoughness.metallicFactor;
                        material.RoughnessFactor = (float)gltfPbrMetallicRoughness.roughnessFactor;
                    }

                    // Emissive
                    {
                        material.EmissiveTextureIndex = PushTextureMapping(gltfMaterial.emissiveTexture.index);

                        BenzinAssert(gltfMaterial.emissiveFactor.size() == 3);
                        material.EmissiveFactor.x = (float)gltfMaterial.emissiveFactor[0];
                        material.EmissiveFactor.y = (float)gltfMaterial.emissiveFactor[1];
                        material.EmissiveFactor.z = (float)gltfMaterial.emissiveFactor[2];
                    }

                    outMeshCollection.Materials.push_back(std::move(material));
                }
            }

            void ParseTextures(MeshCollectionResource& outMeshCollection)
            {
                outMeshCollection.TextureImages.resize(m_TextureMappings.size());

                std::for_each(std::execution::par, m_TextureMappings.begin(), m_TextureMappings.end(), [&](const auto textureMappingEntry)
                {
                    const uint32_t gltfTextureIndex = textureMappingEntry.first;
                    const uint32_t mappedIndex = textureMappingEntry.second;

                    const tinygltf::Texture gltfTexture = m_CurrentModel.textures[gltfTextureIndex];
                    const tinygltf::Image& gltfImage = m_CurrentModel.images[gltfTexture.source];
                    BenzinAssert(gltfImage.bits == 8);
                    BenzinAssert(gltfImage.pixel_type == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE);

                    TextureImage textureImage
                    {
                        .Format = GraphicsFormat::RGBA8Unorm,
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

                    outMeshCollection.TextureImages[mappedIndex] = std::move(textureImage);
                });
            }

            uint32_t PushTextureMapping(int gltfTextureIndex)
            {
                if (gltfTextureIndex == -1)
                {
                    return g_InvalidIndex<uint32_t>;
                }

                if (!m_TextureMappings.contains(gltfTextureIndex))
                {
                    m_TextureMappings[gltfTextureIndex] = (uint32_t)m_TextureMappings.size();
                }

                return m_TextureMappings[gltfTextureIndex];
            }

            void ResetState()
            {
                new (&m_CurrentModel) tinygltf::Model{}; // Reset current model because 'tinygltf' don't reset before loading from file
                m_TextureMappings.clear();
            }

        private:
            tinygltf::Model m_CurrentModel;
            std::unordered_map<uint32_t, uint32_t> m_TextureMappings;
        };

        thread_local GLTFReader g_GLTFReader;

    } // anonymous namespace

    bool LoadMeshCollectionFromGLTFFile(std::string_view fileName, MeshCollectionResource& outMeshCollection)
    {
        return g_GLTFReader.ReadFromFile(fileName, outMeshCollection);
    }

} // namespace benzin
