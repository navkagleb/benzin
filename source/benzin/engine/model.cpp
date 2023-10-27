#include "benzin/config/bootstrap.hpp"
#include "benzin/engine/model.hpp"

#include <third_party/tinygltf/tiny_gltf.h>
#include <third_party/tinygltf/stb_image.h>

#include "benzin/engine/mesh.hpp"
#include "benzin/graphics/command_queue.hpp"
#include "benzin/graphics/device.hpp"
#include "benzin/graphics/mapped_data.hpp"

namespace benzin
{

    namespace
    {

        struct GPUNode
        {
            DirectX::XMMATRIX TransformMatrix;
            DirectX::XMMATRIX InverseTransformMatrix;
        };

        struct GLTFParserResult
        {
            std::vector<MeshData> Meshes;
            std::vector<DrawableMesh> DrawableMeshes;
            std::vector<IterableRange<uint32_t>> DrawableMeshIndexRanges;

            std::vector<Model::Node> ModelNodes;

            std::vector<TextureImage> TextureImages;
            std::vector<Material> Materials;
        };

        class GLTFParser
        {
        private:
            static inline tinygltf::TinyGLTF ms_GLTFContext;

        public:
            std::expected<GLTFParserResult, std::string> Parse(std::string_view fileName)
            {
                const std::filesystem::path filePath = config::g_ModelDirPath / fileName;
                BenzinAssert(std::filesystem::exists(filePath));
                BenzinAssert(filePath.extension() == ".glb" || filePath.extension() == ".gltf");

                std::string error;
                std::string warning;
                bool result = false;

                if (filePath.extension() == ".glb")
                {
                    result = ms_GLTFContext.LoadBinaryFromFile(&m_GLTFModel, &error, &warning, filePath.string());
                }
                else if (filePath.extension() == ".gltf")
                {
                    result = ms_GLTFContext.LoadASCIIFromFile(&m_GLTFModel, &error, &warning, filePath.string());
                }

                if (!warning.empty())
                {
                    return std::unexpected{ warning };
                }
                else if (!error.empty())
                {
                    return std::unexpected{ error };
                }
                else if (!result)
                {
                    return std::unexpected{ "Failed to parse glTF file!"s };
                }

                GLTFParserResult gltfResult;
                ParseGLTFMeshPrimitives(gltfResult);
                ParseGLTFNodes(gltfResult);
                ParseGLTFTextures(gltfResult);
                ParseGLTFMaterials(gltfResult);

                return gltfResult;
            }

        private:
            template <typename T>
            std::span<const T> GetBufferFromGLTFAccessor(int accessorIndex)
            {
                if (accessorIndex == -1)
                {
                    return {};
                }

                const tinygltf::Accessor& gltfAccessor = m_GLTFModel.accessors[accessorIndex];
                const tinygltf::BufferView& gltfBufferView = m_GLTFModel.bufferViews[gltfAccessor.bufferView];
                const tinygltf::Buffer& gltfBuffer = m_GLTFModel.buffers[gltfBufferView.buffer];

                const size_t offsetInBytes = gltfBufferView.byteOffset + gltfAccessor.byteOffset;

                return std::span
                { 
                    reinterpret_cast<const T*>(gltfBuffer.data.data() + offsetInBytes),
                    gltfAccessor.count
                };
            };

            template <std::integral IndexType>
            void ParseGLTFMeshPrimitive(const tinygltf::Primitive& gltfPrimitive, GLTFParserResult& result)
            {
                MeshData& mesh = result.Meshes.emplace_back();
                DrawableMesh& drawableMesh = result.DrawableMeshes.emplace_back();

                BenzinAssert(gltfPrimitive.material != -1);
                drawableMesh.MeshIndex = static_cast<uint32_t>(result.Meshes.size()) - 1;
                drawableMesh.MaterialIndex = gltfPrimitive.material;

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

                const int indexAccessorIndex = gltfPrimitive.indices;
                const int positionAccessorIndex = gltfPrimitive.attributes.contains("POSITION") ? gltfPrimitive.attributes.at("POSITION") : -1;
                const int normalAccessorIndex = gltfPrimitive.attributes.contains("NORMAL") ? gltfPrimitive.attributes.at("NORMAL") : -1;
                const int texCoordAccessorIndex = gltfPrimitive.attributes.contains("TEXCOORD_0") ? gltfPrimitive.attributes.at("TEXCOORD_0") : -1;
                BenzinAssert(!gltfPrimitive.attributes.contains("TEXCOORD_1")); // TODO

                const auto indices = GetBufferFromGLTFAccessor<IndexType>(indexAccessorIndex);
                const auto positions = GetBufferFromGLTFAccessor<DirectX::XMFLOAT3>(positionAccessorIndex);
                const auto normals = GetBufferFromGLTFAccessor<DirectX::XMFLOAT3>(normalAccessorIndex);
                const auto uvs = GetBufferFromGLTFAccessor<DirectX::XMFLOAT2>(texCoordAccessorIndex);

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
                        meshVertex.TexCoord = uvs[i];
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
            }

            void ParseGLTFMesh(const tinygltf::Mesh& gltfMesh, GLTFParserResult& result)
            {
                for (const tinygltf::Primitive& gltfPrimitive : gltfMesh.primitives)
                {
                    BenzinAssert(gltfPrimitive.indices != -1);
                    const tinygltf::Accessor& indexAccessor = m_GLTFModel.accessors[gltfPrimitive.indices];

                    switch (indexAccessor.componentType)
                    {
                        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
                        {
                            ParseGLTFMeshPrimitive<uint8_t>(gltfPrimitive, result);
                            break;
                        }
                        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
                        {
                            ParseGLTFMeshPrimitive<uint16_t>(gltfPrimitive, result);
                            break;
                        }
                        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
                        {
                            ParseGLTFMeshPrimitive<uint32_t>(gltfPrimitive, result);
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

            void ParseGLTFMeshPrimitives(GLTFParserResult& result)
            {
                BenzinAssert(result.Meshes.empty());
                BenzinAssert(result.DrawableMeshes.empty());
                BenzinAssert(result.DrawableMeshIndexRanges.empty());

                result.DrawableMeshIndexRanges.reserve(m_GLTFModel.meshes.size());
                for (const tinygltf::Mesh& gltfMesh : m_GLTFModel.meshes)
                {
                    const auto meshIndex = static_cast<uint32_t>(result.Meshes.size());
                    const auto meshCount = static_cast<uint32_t>(gltfMesh.primitives.size());
                    result.DrawableMeshIndexRanges.emplace_back(meshIndex, meshIndex + meshCount);

                    ParseGLTFMesh(gltfMesh, result);
                }
            }

            static DirectX::XMMATRIX ParseGLTFNodeTransform(const tinygltf::Node& gltfNode, const DirectX::XMMATRIX& parentNodeTransform)
            {
                DirectX::XMMATRIX nodeTransform = DirectX::XMMatrixIdentity();

                if (!gltfNode.matrix.empty())
                {
                    BenzinAssert(gltfNode.matrix.size() == 16);
                    memcpy(&nodeTransform, gltfNode.matrix.data(), sizeof(DirectX::XMMATRIX));
                    nodeTransform = DirectX::XMMatrixTranspose(nodeTransform);
                }
                else
                {
                    if (!gltfNode.rotation.empty())
                    {
                        BenzinAssert(gltfNode.rotation.size() == 4);
                        const DirectX::XMFLOAT4 rotation
                        {
                            static_cast<float>(gltfNode.rotation[0]),
                            static_cast<float>(gltfNode.rotation[1]),
                            static_cast<float>(gltfNode.rotation[2]),
                            static_cast<float>(gltfNode.rotation[3])
                        };

                        nodeTransform *= DirectX::XMMatrixRotationQuaternion(DirectX::XMLoadFloat4(&rotation));
                    }

                    if (!gltfNode.scale.empty())
                    {
                        BenzinAssert(gltfNode.scale.size() == 3);
                        const DirectX::XMFLOAT3 scale
                        {
                            static_cast<float>(gltfNode.scale[0]),
                            static_cast<float>(gltfNode.scale[1]),
                            static_cast<float>(gltfNode.scale[2])
                        };

                        nodeTransform *= DirectX::XMMatrixScalingFromVector(DirectX::XMLoadFloat3(&scale));
                    }

                    if (!gltfNode.translation.empty())
                    {
                        BenzinAssert(gltfNode.translation.size() == 3);
                        const DirectX::XMFLOAT3 translation
                        {
                            static_cast<float>(gltfNode.translation[0]),
                            static_cast<float>(gltfNode.translation[1]),
                            static_cast<float>(gltfNode.translation[2])
                        };

                        nodeTransform *= DirectX::XMMatrixTranslationFromVector(DirectX::XMLoadFloat3(&translation));
                    }
                }

                return nodeTransform * parentNodeTransform;
            }

            void ParseGLTFNode(int nodeIndex, int parentNodeIndex, const DirectX::XMMATRIX& parentNodeTransform, GLTFParserResult& result)
            {
                const tinygltf::Node& gltfNode = m_GLTFModel.nodes[nodeIndex];

                const DirectX::XMMATRIX nodeTransform = ParseGLTFNodeTransform(gltfNode, parentNodeTransform);
                const int meshIndex = gltfNode.mesh;

                if (meshIndex != -1)
                {
                    result.ModelNodes.emplace_back(result.DrawableMeshIndexRanges[meshIndex], nodeTransform);
                }

                for (const int childNodeIndex : gltfNode.children)
                {
                    ParseGLTFNode(childNodeIndex, nodeIndex, nodeTransform, result);
                }
            }

            void ParseGLTFNodes(GLTFParserResult& result)
            {
                BenzinAssert(!result.DrawableMeshIndexRanges.empty());
                BenzinAssert(result.ModelNodes.empty());

                const int parentNodeIndex = -1;

                // Convert from right-handed to left-handed
                // Must be used with TriangleOrder::CounterClockwise in rasterizer state
                const DirectX::XMMATRIX parentNodeTransform = DirectX::XMMatrixScaling(1.0f, 1.0f, -1.0f);

                for (const tinygltf::Scene& gltfScene : m_GLTFModel.scenes)
                {
                    for (const int nodeIndex : gltfScene.nodes)
                    {
                        ParseGLTFNode(nodeIndex, parentNodeIndex, parentNodeTransform, result);
                    }
                }
            }

            void ParseGLTFTextures(GLTFParserResult& result)
            {
                BenzinAssert(result.TextureImages.empty());

                result.TextureImages.resize(m_GLTFModel.textures.size());
                for (const auto& [gltfTexture, textureImage] : std::views::zip(m_GLTFModel.textures, result.TextureImages))
                {
                    const tinygltf::Image& gltfImage = m_GLTFModel.images[gltfTexture.source];
                    BenzinAssert(gltfImage.bits == 8);
                    BenzinAssert(gltfImage.pixel_type == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE);

                    TextureCreation& textureCreation = textureImage.TextureCreation;

                    // Fill 'textureCreation'
                    textureCreation = TextureCreation
                    {
                        .Type = TextureType::Texture2D,
                        .Format = GraphicsFormat::RGBA8Unorm,
                        .Width = static_cast<uint32_t>(gltfImage.width),
                        .Height = static_cast<uint32_t>(gltfImage.height),
                        .MipCount = 1, // TODO: Mip generation
                    };

                    if (!gltfTexture.name.empty())
                    {
                        textureCreation.DebugName.Chars = gltfTexture.name;
                    }
                    else if (!gltfImage.name.empty())
                    {
                        textureCreation.DebugName.Chars = gltfImage.name;
                    }
                    else if (!gltfImage.uri.empty())
                    {
                        textureCreation.DebugName.Chars = gltfImage.uri;
                    }
                    
                    // Fill image
                    textureImage.ImageData.resize(gltfImage.image.size());
                    memcpy(textureImage.ImageData.data(), gltfImage.image.data(), gltfImage.image.size());
                }
            }

            void ParseGLTFMaterials(GLTFParserResult& result)
            {
                BenzinAssert(result.Materials.empty());

                result.Materials.resize(m_GLTFModel.materials.size());
                for (const auto& [gltfMaterial, material] : std::views::zip(m_GLTFModel.materials, result.Materials))
                {
                    const tinygltf::PbrMetallicRoughness& gltfPbrMetallicRoughness = gltfMaterial.pbrMetallicRoughness;

                    // Albedo
                    {
                        const int albedoTextureIndex = gltfPbrMetallicRoughness.baseColorTexture.index;

                        if (albedoTextureIndex != -1)
                        {
                            material.AlbedoTextureIndex = static_cast<uint32_t>(albedoTextureIndex);
                        }

                        BenzinAssert(gltfPbrMetallicRoughness.baseColorFactor.size() == 4);
                        material.AlbedoFactor.x = static_cast<float>(gltfPbrMetallicRoughness.baseColorFactor[0]);
                        material.AlbedoFactor.y = static_cast<float>(gltfPbrMetallicRoughness.baseColorFactor[1]);
                        material.AlbedoFactor.z = static_cast<float>(gltfPbrMetallicRoughness.baseColorFactor[2]);
                        material.AlbedoFactor.w = static_cast<float>(gltfPbrMetallicRoughness.baseColorFactor[3]);

                        material.AlphaCutoff = static_cast<float>(gltfMaterial.alphaCutoff);
                    }

                    // Normal
                    {
                        const int normalTextureIndex = gltfMaterial.normalTexture.index;

                        if (normalTextureIndex != -1)
                        {
                            material.NormalTextureIndex = static_cast<uint32_t>(normalTextureIndex);
                        }

                        material.NormalScale = static_cast<float>(gltfMaterial.normalTexture.scale);
                    }

                    // MetalRoughness
                    {
                        const int metalRoughessTextureIndex = gltfPbrMetallicRoughness.metallicRoughnessTexture.index;

                        if (metalRoughessTextureIndex != -1)
                        {
                            material.MetalRoughnessTextureIndex = static_cast<uint32_t>(metalRoughessTextureIndex);
                        }

                        material.MetalnessFactor = static_cast<float>(gltfPbrMetallicRoughness.metallicFactor);
                        material.RoughnessFactor = static_cast<float>(gltfPbrMetallicRoughness.roughnessFactor);
                    }

                    // AO
                    {
                        const int aoTextureIndex = gltfMaterial.occlusionTexture.index;

                        if (aoTextureIndex != -1)
                        {
                            material.AOTextureIndex = static_cast<uint32_t>(aoTextureIndex);
                        }
                    }

                    // Emissive
                    {
                        const int emissiveTextureIndex = gltfMaterial.emissiveTexture.index;

                        if (emissiveTextureIndex != -1)
                        {
                            material.EmissiveTextureIndex = static_cast<uint32_t>(emissiveTextureIndex);
                        }

                        BenzinAssert(gltfMaterial.emissiveFactor.size() == 3);
                        material.EmissiveFactor.x = static_cast<float>(gltfMaterial.emissiveFactor[0]);
                        material.EmissiveFactor.y = static_cast<float>(gltfMaterial.emissiveFactor[1]);
                        material.EmissiveFactor.z = static_cast<float>(gltfMaterial.emissiveFactor[2]);
                    }
                }
            }

        private:
            tinygltf::Model m_GLTFModel;
        };

        GLTFParser g_GLTFParser;

    } // anonymous namespace

    Model::Model(Device& device)
        : m_Device{ device }
    {}

    Model::Model(Device& device, std::string_view fileName)
        : Model{ device }
    {
        LoadFromFile(fileName);
    }

    Model::Model(Device& device, const ModelCreation& creation)
        : Model{ device }
    {
        Create(creation);
    }

    void Model::LoadFromFile(std::string_view fileName)
    {
        BenzinAssert(!m_Mesh.get());
        BenzinAssert(m_DrawableMeshes.empty());
        BenzinAssert(m_Nodes.empty());
        BenzinAssert(m_Materials.empty());

        GLTFParser gltfParser;
        const auto gltfResult = gltfParser.Parse(fileName);

        if (!gltfResult)
        {
            BenzinError("GLTFParser Error: {}", gltfResult.error());
            return;
        }

        m_DebugName = CutExtension(fileName);

        m_Mesh = std::make_shared<Mesh>(m_Device, MeshCreation
        {
            .DebugName = m_DebugName,
            .Meshes = gltfResult->Meshes,
            .IsNeedSplitByMeshes = true,
        });

        m_DrawableMeshes = std::move(gltfResult->DrawableMeshes);
        m_Nodes = std::move(gltfResult->ModelNodes);
        m_Materials = std::move(gltfResult->Materials);

        CreateDrawableMeshBuffer();
        CreateNodeBuffer();
        CreateTextures(gltfResult->TextureImages);
        CreateMaterialBuffer();
    }

    void Model::Create(const ModelCreation& creation)
    {
        BenzinAssert(!m_Mesh.get());
        BenzinAssert(m_DrawableMeshes.empty());
        BenzinAssert(m_Nodes.empty());
        BenzinAssert(m_Materials.empty());

        m_DebugName = creation.DebugName;

        m_Mesh = creation.Mesh;
        m_DrawableMeshes = creation.DrawableMeshes;
        m_Nodes.emplace_back(IterableRange<uint32_t>{ 0, static_cast<uint32_t>(creation.DrawableMeshes.size()) });
        m_Materials = creation.Materials;

        CreateDrawableMeshBuffer();
        CreateNodeBuffer();
        CreateMaterialBuffer();
    }

    void Model::CreateDrawableMeshBuffer()
    {
        BenzinAssert(!m_DrawableMeshes.empty());

        m_DrawableMeshBuffer = std::make_unique<Buffer>(m_Device, BufferCreation
        {
            .DebugName = std::format("{}_{}", m_DebugName, "DrawableMeshBuffer"),
            .ElementSize = sizeof(DrawableMesh),
            .ElementCount = static_cast<uint32_t>(m_DrawableMeshes.size()),
            .IsNeedShaderResourceView = true,
        });

        auto& copyCommandQueue = m_Device.GetCopyCommandQueue();
        BenzinFlushCommandQueueOnScopeExit(copyCommandQueue);

        auto& copyCommandList = copyCommandQueue.GetCommandList(m_DrawableMeshBuffer->GetSizeInBytes());
        copyCommandList.UpdateBuffer(*m_DrawableMeshBuffer, std::span<const DrawableMesh>{ m_DrawableMeshes });
    }

    void Model::CreateNodeBuffer()
    {
        BenzinAssert(!m_Nodes.empty());

        m_NodeBuffer = std::make_shared<Buffer>(m_Device, BufferCreation
        {
            .DebugName = std::format("{}_{}", m_DebugName, "NodeBuffer"),
            .ElementSize = sizeof(GPUNode),
            .ElementCount = static_cast<uint32_t>(m_Nodes.size()),
            .IsNeedShaderResourceView = true,
        });

        auto& copyCommandQueue = m_Device.GetCopyCommandQueue();
        BenzinFlushCommandQueueOnScopeExit(copyCommandQueue);

        auto& copyCommandList = copyCommandQueue.GetCommandList(m_NodeBuffer->GetSizeInBytes());

        for (const auto& [i, node] : m_Nodes | std::views::enumerate)
        {
            DirectX::XMVECTOR transformDeterminant = DirectX::XMMatrixDeterminant(node.Transform);
            const DirectX::XMMATRIX inverseTransform = DirectX::XMMatrixInverse(&transformDeterminant, node.Transform);

            const GPUNode gpuNode
            {
                .TransformMatrix = node.Transform,
                .InverseTransformMatrix = inverseTransform,
            };

            copyCommandList.UpdateBuffer(*m_NodeBuffer, std::span{ &gpuNode, 1 }, i);
        }
    }

    void Model::CreateTextures(const std::vector<TextureImage>& textureImages)
    {
        uint32_t uploadBufferSize = 0;

        m_Textures.reserve(textureImages.size());
        for (const auto& textureImage : textureImages)
        {
            const TextureCreation& textureCreation = textureImage.TextureCreation;
            BenzinAssert(textureCreation.Format == GraphicsFormat::RGBA8Unorm);

            auto& texture = m_Textures.emplace_back();
            if (textureCreation.DebugName.IsEmpty())
            {
                texture = std::make_shared<Texture>(m_Device, textureCreation);
            }
            else
            {
                // Because of life time 'TextureCreation::DebugName::Chars'
                const std::string debugName = std::format("{}_{}", m_DebugName, textureCreation.DebugName.Chars);

                TextureCreation validatedTextureCreation = textureCreation;
                validatedTextureCreation.DebugName.Chars = debugName;

                texture = std::make_shared<Texture>(m_Device, validatedTextureCreation);
            }

            texture->PushShaderResourceView();

            uploadBufferSize += AlignAbove(texture->GetSizeInBytes(), config::g_TextureAlignment);
        }

        auto& copyCommandQueue = m_Device.GetCopyCommandQueue();
        BenzinFlushCommandQueueOnScopeExit(copyCommandQueue);

        auto& copyCommandList = copyCommandQueue.GetCommandList(uploadBufferSize);

        for (const auto& [textureImage, texture] : std::views::zip(textureImages, m_Textures))
        {
            copyCommandList.UpdateTextureTopMip(*texture, textureImage.ImageData.data());
        }
    }

    void Model::CreateMaterialBuffer()
    {
        // NOTE: If use static labmda don't use [&, this] in capture
        static constexpr auto UpdateMaterialTextureIndex = [](const std::vector<std::shared_ptr<Texture>>& textures, uint32_t& textureIndex)
        {
            if (textureIndex != -1)
            {
                BenzinAssert(textureIndex < textures.size());
                textureIndex = textures[textureIndex]->GetShaderResourceView().GetHeapIndex();
            }
        };

        BenzinAssert(!m_Materials.empty());

        for (const auto& material : m_Materials)
        {
            UpdateMaterialTextureIndex(m_Textures, const_cast<uint32_t&>(material.AlbedoTextureIndex));
            UpdateMaterialTextureIndex(m_Textures, const_cast<uint32_t&>(material.NormalTextureIndex));
            UpdateMaterialTextureIndex(m_Textures, const_cast<uint32_t&>(material.MetalRoughnessTextureIndex));
            UpdateMaterialTextureIndex(m_Textures, const_cast<uint32_t&>(material.AOTextureIndex));
            UpdateMaterialTextureIndex(m_Textures, const_cast<uint32_t&>(material.EmissiveTextureIndex));
        }

        m_MaterialBuffer = std::make_shared<Buffer>(m_Device, BufferCreation
        {
            .DebugName = std::format("{}_{}", m_DebugName, "MaterialBuffer"),
            .ElementSize = sizeof(Material),
            .ElementCount = static_cast<uint32_t>(m_Materials.size()),
            .IsNeedShaderResourceView = true,
        });

        auto& copyCommandQueue = m_Device.GetCopyCommandQueue();
        BenzinFlushCommandQueueOnScopeExit(copyCommandQueue);

        auto& copyCommandList = copyCommandQueue.GetCommandList(m_MaterialBuffer->GetSizeInBytes());
        copyCommandList.UpdateBuffer(*m_MaterialBuffer, std::span<const Material>{ m_Materials });
    }

} // namespace benzin
