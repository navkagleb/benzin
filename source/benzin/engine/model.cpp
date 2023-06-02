#include "benzin/config/bootstrap.hpp"

#include "benzin/engine/model.hpp"

#include <third_party/tinygltf/tiny_gltf.h>
#include <third_party/tinygltf/stb_image.h>

#include "benzin/graphics/api/command_queue.hpp"
#include "benzin/graphics/api/device.hpp"
#include "benzin/graphics/mapped_data.hpp"

namespace benzin
{

    namespace
    {

        struct GPUPrimitive
        {
            uint32_t StartVertex;
            uint32_t StartIndex;
            uint32_t IndexCount;
        };

        using GPUMaterial = MaterialData;

        struct GPUNode
        {
            DirectX::XMMATRIX TransformMatrix;
            DirectX::XMMATRIX InverseTransformMatrix;
        };

        const std::filesystem::path g_ModelsRootPath{ "../../assets/models/" };

        std::optional<tinygltf::Model> LoadGLTFModelFromFile(std::string_view fileName)
        {
            tinygltf::TinyGLTF g_GLTFContext;

            const std::filesystem::path filePath = g_ModelsRootPath / fileName;
            BENZIN_ASSERT(std::filesystem::exists(filePath));
            BENZIN_ASSERT(filePath.extension() == ".glb" || filePath.extension() == ".gltf");

            tinygltf::Model gltfModel;
            std::string error;
            std::string warning;
            bool result = false;

            if (filePath.extension() == ".glb")
            {
                result = g_GLTFContext.LoadBinaryFromFile(&gltfModel, &error, &warning, filePath.string());
            }
            else if (filePath.extension() == ".gltf")
            {
                result = g_GLTFContext.LoadASCIIFromFile(&gltfModel, &error, &warning, filePath.string());
            }

            if (!warning.empty())
            {
                BENZIN_WARNING("GLTF Loader: {}", warning);
            }

            if (!error.empty())
            {
                BENZIN_ERROR("GLTF Loader: {}", error);
            }

            if (!result)
            {
                BENZIN_ERROR("Failed to parse glTF file!");
                return std::nullopt;
            }

            return gltfModel;
        }

        template <typename T>
        std::span<const T> GetBufferFromGLTFAccessor(const tinygltf::Model& gltfModel, int accessorIndex)
        {
            if (accessorIndex == -1)
            {
                return {};
            }

            const tinygltf::Accessor& gltfAccessor = gltfModel.accessors[accessorIndex];
            const tinygltf::BufferView& gltfBufferView = gltfModel.bufferViews[gltfAccessor.bufferView];
            const tinygltf::Buffer& gltfBuffer = gltfModel.buffers[gltfBufferView.buffer];

            const size_t offsetInBytes = gltfBufferView.byteOffset + gltfAccessor.byteOffset;

            return std::span<const T>
            {
                reinterpret_cast<const T*>(gltfBuffer.data.data() + offsetInBytes),
                    gltfAccessor.count
            };
        };

        template <std::integral IndexType>
        void ParseGLTFMeshPrimitive(
            const tinygltf::Model& gltfModel,
            const tinygltf::Primitive& gltfPrimitive,
            std::vector<MeshPrimitiveData>& meshPrimitivesData,
            std::vector<Model::DrawPrimitive>& drawPrimitives
        )
        {
            MeshPrimitiveData& meshPrimitiveData = meshPrimitivesData.emplace_back();
            Model::DrawPrimitive& drawPrimitive = drawPrimitives.emplace_back();

            BENZIN_ASSERT(gltfPrimitive.material != -1);
            drawPrimitive.MeshPrimitiveIndex = static_cast<uint32_t>(meshPrimitivesData.size()) - 1; // Because 'meshPrimitivesData.emplace_back()'
            drawPrimitive.MaterialIndex = gltfPrimitive.material;

            switch (gltfPrimitive.mode)
            {
                case TINYGLTF_MODE_TRIANGLES:
                {
                    meshPrimitiveData.PrimitiveTopology = PrimitiveTopology::TriangleList;
                    break;
                }
                case TINYGLTF_MODE_TRIANGLE_STRIP:
                {
                    meshPrimitiveData.PrimitiveTopology = PrimitiveTopology::TriangleStrip;
                    break;
                }
                default:
                {
                    BENZIN_ASSERT(false);
                    break;
                }
            }

            const int indexAccessorIndex = gltfPrimitive.indices;
            const int positionAccessorIndex = gltfPrimitive.attributes.contains("POSITION") ? gltfPrimitive.attributes.at("POSITION") : -1;
            const int normalAccessorIndex = gltfPrimitive.attributes.contains("NORMAL") ? gltfPrimitive.attributes.at("NORMAL") : -1;
            const int texCoordAccessorIndex = gltfPrimitive.attributes.contains("TEXCOORD_0") ? gltfPrimitive.attributes.at("TEXCOORD_0") : -1;

            const std::span<const IndexType> indices = GetBufferFromGLTFAccessor<IndexType>(gltfModel, indexAccessorIndex);
            const std::span<const DirectX::XMFLOAT3> positions = GetBufferFromGLTFAccessor<DirectX::XMFLOAT3>(gltfModel, positionAccessorIndex);
            const std::span<const DirectX::XMFLOAT3> normals = GetBufferFromGLTFAccessor<DirectX::XMFLOAT3>(gltfModel, normalAccessorIndex);
            const std::span<const DirectX::XMFLOAT2> texCoords = GetBufferFromGLTFAccessor<DirectX::XMFLOAT2>(gltfModel, texCoordAccessorIndex);

            BENZIN_ASSERT(!positions.empty());

            if (!normals.empty())
            {
                BENZIN_ASSERT(normals.size() == positions.size());
            }

            if (!texCoords.empty())
            {
                BENZIN_ASSERT(texCoords.size() == positions.size());
            }

            meshPrimitiveData.Vertices.resize(positions.size());
            for (size_t i = 0; i < meshPrimitiveData.Vertices.size(); ++i)
            {
                MeshVertexData& meshVertexData = meshPrimitiveData.Vertices[i];
                meshVertexData.LocalPosition = positions[i];

                if (!normals.empty())
                {
                    meshVertexData.LocalNormal = normals[i];
                }

                if (!texCoords.empty())
                {
                    meshVertexData.TexCoord = texCoords[i];
                }
            }

            meshPrimitiveData.Indices.assign(indices.begin(), indices.end());
        }

        void ParseGLTFMesh(
            const tinygltf::Model& gltfModel,
            const tinygltf::Mesh& gltfMesh,
            std::vector<MeshPrimitiveData>& meshPrimitivesData,
            std::vector<Model::DrawPrimitive>& drawPrimitives
        )
        {
            for (const tinygltf::Primitive& gltfPrimitive : gltfMesh.primitives)
            {
                BENZIN_ASSERT(gltfPrimitive.indices != -1);
                const tinygltf::Accessor& indexAccessor = gltfModel.accessors[gltfPrimitive.indices];

                switch (indexAccessor.componentType)
                {
                    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
                    {
                        ParseGLTFMeshPrimitive<uint16_t>(gltfModel, gltfPrimitive, meshPrimitivesData, drawPrimitives);
                        break;
                    }
                    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
                    {
                        ParseGLTFMeshPrimitive<uint32_t>(gltfModel, gltfPrimitive, meshPrimitivesData, drawPrimitives);
                        break;
                    }
                    default:
                    {
                        BENZIN_ASSERT(false);
                        break;
                    }
                }
            }
        }

        void ParseGLTFMeshPrimitives(
            const tinygltf::Model& gltfModel,
            std::vector<MeshPrimitiveData>& meshPrimitivesData,
            std::vector<Model::DrawPrimitive>& drawPrimitives,
            std::vector<IterableRange<uint32_t>>& meshPrimitivesRanges
        )
        {
            BENZIN_ASSERT(meshPrimitivesData.empty());
            BENZIN_ASSERT(drawPrimitives.empty());
            BENZIN_ASSERT(meshPrimitivesRanges.empty());

            meshPrimitivesRanges.reserve(gltfModel.meshes.size());

            for (const tinygltf::Mesh& gltfMesh : gltfModel.meshes)
            {
                meshPrimitivesRanges.emplace_back(
                    static_cast<uint32_t>(meshPrimitivesData.size()),
                    static_cast<uint32_t>(meshPrimitivesData.size() + gltfMesh.primitives.size())
                );

                ParseGLTFMesh(gltfModel, gltfMesh, meshPrimitivesData, drawPrimitives);
            }
        }

        DirectX::XMMATRIX ParseGLTFNodeTransform(const tinygltf::Node& gltfNode, const DirectX::XMMATRIX& parentNodeTransform)
        {
            DirectX::XMMATRIX nodeTransform = DirectX::XMMatrixIdentity();

            if (!gltfNode.matrix.empty())
            {
                BENZIN_ASSERT(gltfNode.matrix.size() == 16);
                memcpy(&nodeTransform, gltfNode.matrix.data(), sizeof(DirectX::XMMATRIX));
                nodeTransform = DirectX::XMMatrixTranspose(nodeTransform);
            }
            else
            {
                if (!gltfNode.rotation.empty())
                {
                    BENZIN_ASSERT(gltfNode.rotation.size() == 4);
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
                    BENZIN_ASSERT(gltfNode.scale.size() == 3);
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
                    BENZIN_ASSERT(gltfNode.translation.size() == 3);
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

        void ParseGLTFNode(
            const tinygltf::Model& gltfModel,
            int nodeIndex,
            int parentNodeIndex,
            const DirectX::XMMATRIX& parentNodeTransform,
            const std::vector<IterableRange<uint32_t>>& meshPrimitiveRanges,
            std::vector<NodeData>& nodesData
        )
        {
            const tinygltf::Node& gltfNode = gltfModel.nodes[nodeIndex];

            const DirectX::XMMATRIX nodeTransform = ParseGLTFNodeTransform(gltfNode, parentNodeTransform);
            const int meshIndex = gltfNode.mesh;

            if (meshIndex != -1)
            {
                NodeData& nodeData = nodesData.emplace_back();
                nodeData.DrawPrimitiveRange = meshPrimitiveRanges[meshIndex];
                nodeData.Transform = nodeTransform;
            }

            for (const int childNodeIndex : gltfNode.children)
            {
                ParseGLTFNode(gltfModel, childNodeIndex, nodeIndex, nodeTransform, meshPrimitiveRanges, nodesData);
            }
        }

        void ParseGLTFNodes(
            const tinygltf::Model& gltfModel,
            const std::vector<IterableRange<uint32_t>>& meshPrimitiveRanges,
            std::vector<NodeData>& nodesData
        )
        {
            const int parentNodeIndex = -1;

            // From right-handed to left-handed
            // Must be used with TriangleOrder::CounterClockwise in rasterizer state
            const DirectX::XMMATRIX parentNodeTransform = DirectX::XMMatrixScaling(1.0f, 1.0f, -1.0f);

            for (const tinygltf::Scene& gltfScene : gltfModel.scenes)
            {
                for (const int nodeIndex : gltfScene.nodes)
                {
                    ParseGLTFNode(gltfModel, nodeIndex, parentNodeIndex, parentNodeTransform, meshPrimitiveRanges, nodesData);
                }
            }
        }

        void ParseGLTFTextures(const tinygltf::Model& gltfModel, std::vector<TextureResourceData>& textureResourcesData)
        {
            BENZIN_ASSERT(textureResourcesData.empty());
            textureResourcesData.reserve(gltfModel.textures.size());

            for (const tinygltf::Texture& gltfTexture : gltfModel.textures)
            {
                const tinygltf::Image& gltfImage = gltfModel.images[gltfTexture.source];
                BENZIN_ASSERT(gltfImage.bits == 8);
                BENZIN_ASSERT(gltfImage.pixel_type == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE);

                TextureResourceData& textureResourceData = textureResourcesData.emplace_back();

                if (!gltfTexture.name.empty())
                {
                    textureResourceData.Name = gltfTexture.name;
                }
                else if (!gltfImage.name.empty())
                {
                    textureResourceData.Name = gltfImage.name;
                }
                else if (!gltfImage.uri.empty())
                {
                    textureResourceData.Name = gltfImage.uri;
                }

                const TextureResource::Config config
                {
                    .Type{ TextureResource::Type::Texture2D },
                    .Width{ static_cast<uint32_t>(gltfImage.width) },
                    .Height{ static_cast<uint32_t>(gltfImage.height) },
                    .MipCount{ 1 },
                    .Format{ GraphicsFormat::RGBA8Unorm },
                };

                textureResourceData.Config = config;
                textureResourceData.ImageData.resize(gltfImage.image.size());
                memcpy(textureResourceData.ImageData.data(), gltfImage.image.data(), gltfImage.image.size());
            }
        }

        void ParseGLTFMaterials(const tinygltf::Model& gltfModel, std::vector<MaterialData>& materialsData)
        {
            BENZIN_ASSERT(materialsData.empty());
            materialsData.reserve(gltfModel.materials.size());

            for (const tinygltf::Material& gltfMaterial : gltfModel.materials)
            {
                const tinygltf::PbrMetallicRoughness& gltfPbrMetallicRoughness = gltfMaterial.pbrMetallicRoughness;

                MaterialData& material = materialsData.emplace_back();

                // Albedo
                {
                    const int albedoTextureIndex = gltfPbrMetallicRoughness.baseColorTexture.index;

                    if (albedoTextureIndex != -1)
                    {
                        material.AlbedoTextureIndex = static_cast<uint32_t>(albedoTextureIndex);
                    }

                    BENZIN_ASSERT(gltfPbrMetallicRoughness.baseColorFactor.size() == 4);
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

                    BENZIN_ASSERT(gltfMaterial.emissiveFactor.size() == 3);
                    material.EmissiveFactor.x = static_cast<float>(gltfMaterial.emissiveFactor[0]);
                    material.EmissiveFactor.y = static_cast<float>(gltfMaterial.emissiveFactor[1]);
                    material.EmissiveFactor.z = static_cast<float>(gltfMaterial.emissiveFactor[2]);
                }
            }
        }

    } // anonymous namespace

	Mesh::Mesh(Device& device)
        : m_Device{ device }
    {}

    Mesh::Mesh(Device& device, const std::vector<MeshPrimitiveData>& meshPrimitivesData, std::string_view name)
        : m_Device{ device }
    {
        CreateFromPrimitives(meshPrimitivesData, name);
    }

    void Mesh::CreateFromPrimitives(const std::vector<MeshPrimitiveData>& meshPrimitivesData, std::string_view name)
    {
        BENZIN_ASSERT(!name.empty());

        uint32_t vertexCount = 0;
        uint32_t indexCount = 0;

        for (const auto& meshPrimitiveData : meshPrimitivesData)
        {
            vertexCount += static_cast<uint32_t>(meshPrimitiveData.Vertices.size());
            indexCount += static_cast<uint32_t>(meshPrimitiveData.Indices.size());
        }

        CreateBuffers(vertexCount, indexCount, static_cast<uint32_t>(meshPrimitivesData.size()), name);
        FillBuffers(meshPrimitivesData);
    }

    void Mesh::CreateBuffers(uint32_t vertexCount, uint32_t indexCount, uint32_t primitiveCount, std::string_view name)
    {
        static constexpr auto getBufferDebugName = [](std::string_view meshName, std::string_view bufferDebugName)
        {
            return fmt::format("{}_{}", meshName, bufferDebugName);
        };

        m_VertexBuffer = std::make_shared<BufferResource>(m_Device, BufferResource::Config{ sizeof(MeshVertexData), vertexCount });
        m_VertexBuffer->SetDebugName(getBufferDebugName(name, "VertexBuffer"));
        m_VertexBuffer->PushShaderResourceView();

        m_IndexBuffer = std::make_shared<BufferResource>(m_Device, BufferResource::Config{ sizeof(uint32_t), indexCount });
        m_IndexBuffer->SetDebugName(getBufferDebugName(name, "IndexBuffer"));
        m_IndexBuffer->PushShaderResourceView();

        m_PrimitiveBuffer = std::make_shared<BufferResource>(m_Device, BufferResource::Config{ sizeof(GPUPrimitive), primitiveCount });
        m_PrimitiveBuffer->SetDebugName(getBufferDebugName(name, "MeshPrimitiveBuffer"));
        m_PrimitiveBuffer->PushShaderResourceView();
    }

    void Mesh::FillBuffers(const std::vector<MeshPrimitiveData>& meshPrimitivesData)
    {
        const uint32_t uploadBufferSize = m_VertexBuffer->GetSizeInBytes() + m_IndexBuffer->GetSizeInBytes() + m_PrimitiveBuffer->GetSizeInBytes();

        CommandQueueScope copyCommandQueue{ m_Device.GetCopyCommandQueue() };
        auto& copyCommandList = copyCommandQueue->GetCommandList(uploadBufferSize);

        size_t vertexOffset = 0;
        size_t indexOffset = 0;

        for (size_t i = 0; i < meshPrimitivesData.size(); ++i)
        {
            const auto& meshPrimitiveData = meshPrimitivesData[i];

            const Primitive primitive
            {
                .IndexCount{ static_cast<uint32_t>(meshPrimitiveData.Indices.size()) },
                .PrimitiveTopology{ meshPrimitiveData.PrimitiveTopology },
            };

            m_Primitives.push_back(primitive);

            const GPUPrimitive gpuPrimitive
            {
                .StartVertex{ static_cast<uint32_t>(vertexOffset) },
                .StartIndex{ static_cast<uint32_t>(indexOffset) },
                .IndexCount{ static_cast<uint32_t>(meshPrimitiveData.Indices.size()) },
            };

            copyCommandList.UpdateBuffer(*m_VertexBuffer, std::span{ meshPrimitiveData.Vertices }, vertexOffset);
            copyCommandList.UpdateBuffer(*m_IndexBuffer, std::span{ meshPrimitiveData.Indices }, indexOffset);
            copyCommandList.UpdateBuffer(*m_PrimitiveBuffer, std::span{ &gpuPrimitive, 1 }, i);

            vertexOffset += meshPrimitiveData.Vertices.size();
            indexOffset += meshPrimitiveData.Indices.size();
        }
    }

    Model::Model(Device& device)
        : m_Device{ device }
    {}

    Model::Model(Device& device, std::string_view fileName)
        : Model{ device }
    {
        LoadFromGLTFFile(fileName);
    }

    Model::Model(
        Device& device,
        const std::shared_ptr<Mesh>& mesh,
        const std::vector<DrawPrimitive>& drawPrimitives,
        const std::vector<MaterialData>& materialsData,
        std::string_view name
    )
        : Model{ device }
    {
        CreateFrom(mesh, drawPrimitives, materialsData, name);
    }

    bool Model::LoadFromGLTFFile(std::string_view fileName)
    {
        std::optional<tinygltf::Model> gltfModel = LoadGLTFModelFromFile(fileName);

        if (!gltfModel)
        {
            return false;
        }

        // TODO
        m_Name = std::filesystem::path{ fileName }.replace_extension().filename().string();

        std::vector<MeshPrimitiveData> meshPrimitivesData;
        std::vector<DrawPrimitive> drawPrimitives;
        std::vector<IterableRange<uint32_t>> meshPrimitiveRanges;
        ParseGLTFMeshPrimitives(*gltfModel, meshPrimitivesData, drawPrimitives, meshPrimitiveRanges);

        std::vector<NodeData> nodesData;
        ParseGLTFNodes(*gltfModel, meshPrimitiveRanges, nodesData);

        std::vector<TextureResourceData> textureResourcesData;
        ParseGLTFTextures(*gltfModel, textureResourcesData);

        std::vector<MaterialData> materialsData;
        ParseGLTFMaterials(*gltfModel, materialsData);

        {
            m_Mesh = std::make_shared<Mesh>(m_Device, meshPrimitivesData, m_Name);
            
            CreateDrawPrimitivesBuffer(drawPrimitives);
            CreateNodes(nodesData);
            CreateTextures(textureResourcesData);
            CreateMaterialBuffer(materialsData);
        }
        
        return true;
    }

    void Model::CreateFrom(
        const std::shared_ptr<Mesh>& mesh,
        const std::vector<DrawPrimitive>& drawPrimitives,
        const std::vector<MaterialData>& materialsData,
        std::string_view name
    )
    {
        m_Name = name;

        const NodeData singleNodeData
        {
            .DrawPrimitiveRange{ 0, static_cast<uint32_t>(drawPrimitives.size()) }
        };

        m_Mesh = mesh;
        CreateDrawPrimitivesBuffer(drawPrimitives);
        CreateNodes({ singleNodeData });
        CreateMaterialBuffer(materialsData);
    }

    void Model::CreateDrawPrimitivesBuffer(const std::vector<DrawPrimitive>& drawPrimitives)
    {
        m_DrawPrimitiveBuffer = std::make_unique<BufferResource>(m_Device, BufferResource::Config{ sizeof(DrawPrimitive), static_cast<uint32_t>(drawPrimitives.size()) });
        m_DrawPrimitiveBuffer->SetDebugName(fmt::format("{}_{}", m_Name, "DrawPrimitive"));
        m_DrawPrimitiveBuffer->PushShaderResourceView();

        {
            CommandQueueScope copyCommandQueue{ m_Device.GetCopyCommandQueue() };
            auto& copyCommandList = copyCommandQueue->GetCommandList(m_DrawPrimitiveBuffer->GetSizeInBytes());
            copyCommandList.UpdateBuffer(*m_DrawPrimitiveBuffer, std::span{ drawPrimitives });
        }

        m_DrawPrimitives = drawPrimitives;
    }

    void Model::CreateNodes(const std::vector<NodeData>& nodesData)
    {
        m_Nodes.reserve(nodesData.size());

        m_NodeBuffer = std::make_shared<BufferResource>(m_Device, BufferResource::Config{ sizeof(GPUNode), static_cast<uint32_t>(nodesData.size()) });
        m_NodeBuffer->SetDebugName(fmt::format("{}_{}", m_Name, "NodeBuffer"));
        m_NodeBuffer->PushShaderResourceView();

        CommandQueueScope copyCommandQueue{ m_Device.GetCopyCommandQueue() };
        auto& copyCommandList = copyCommandQueue->GetCommandList(m_NodeBuffer->GetSizeInBytes());

        for (size_t i = 0; i < nodesData.size(); ++i)
        {
            const auto& nodeData = nodesData[i];

            m_Nodes.emplace_back(nodeData.DrawPrimitiveRange);

            {
                DirectX::XMVECTOR transformDeterminant = DirectX::XMMatrixDeterminant(nodeData.Transform);
                const DirectX::XMMATRIX inverseTransform = DirectX::XMMatrixInverse(&transformDeterminant, nodeData.Transform);

                const GPUNode gpuNode
                {
                    .TransformMatrix{ nodeData.Transform },
                    .InverseTransformMatrix{ inverseTransform },
                };

                copyCommandList.UpdateBuffer(*m_NodeBuffer, std::span{ &gpuNode, 1 }, i);
            }
        }
    }

    void Model::CreateTextures(const std::vector<TextureResourceData>& textureResourcesData)
    {
        uint32_t uploadBufferSize = 0;

        m_Textures.reserve(textureResourcesData.size());
        for (const auto& textureResourceData : textureResourcesData)
        {
            const TextureResource::Config& config = textureResourceData.Config;
            BENZIN_ASSERT(config.Format == GraphicsFormat::RGBA8Unorm);

            auto& texture = m_Textures.emplace_back();
            texture = std::make_shared<TextureResource>(m_Device, config);
            texture->PushShaderResourceView();

            if (!textureResourceData.Name.empty())
            {
                texture->SetDebugName(fmt::format("{}_{}", m_Name, textureResourceData.Name));
            }

            uploadBufferSize += texture->GetSizeInBytes();
        }

        CommandQueueScope copyCommandQueue{ m_Device.GetCopyCommandQueue() };
        auto& copyCommandList = copyCommandQueue->GetCommandList(uploadBufferSize);

        for (size_t i = 0; i < m_Textures.size(); ++i)
        {
            const auto& textureResourceData = textureResourcesData[i];
            auto& texture = m_Textures[i];

            copyCommandList.UpdateTextureTopMip(*texture, textureResourceData.ImageData.data());
        }
    }

    void Model::CreateMaterialBuffer(const std::vector<MaterialData>& materialsData)
    {
        // NOTE: If use static labmda don't use [&, this] in caputer
        static constexpr auto updateMaterialTextureIndex = [](const std::vector<std::shared_ptr<TextureResource>>& textures, uint32_t& textureIndex)
        {
            if (textureIndex != -1)
            {
                BENZIN_ASSERT(textureIndex < textures.size());
                textureIndex = textures[textureIndex]->GetShaderResourceView().GetHeapIndex();
            }
        };

        for (const auto& materialData : materialsData)
        {
            updateMaterialTextureIndex(m_Textures, const_cast<uint32_t&>(materialData.AlbedoTextureIndex));
            updateMaterialTextureIndex(m_Textures, const_cast<uint32_t&>(materialData.NormalTextureIndex));
            updateMaterialTextureIndex(m_Textures, const_cast<uint32_t&>(materialData.MetalRoughnessTextureIndex));
            updateMaterialTextureIndex(m_Textures, const_cast<uint32_t&>(materialData.AOTextureIndex));
            updateMaterialTextureIndex(m_Textures, const_cast<uint32_t&>(materialData.EmissiveTextureIndex));
        }

        m_Materials = materialsData;

        m_MaterialBuffer = std::make_shared<BufferResource>(m_Device, BufferResource::Config
        {
            .ElementSize{ sizeof(GPUMaterial) },
            .ElementCount{ static_cast<uint32_t>(materialsData.size()) },
            .Flags{ BufferResource::Flags::Dynamic },
        });
        m_MaterialBuffer->SetDebugName(fmt::format("{}_{}", m_Name, "MaterialBuffer"));
        m_MaterialBuffer->PushShaderResourceView();

        {
            CommandQueueScope copyCommandQueue{ m_Device.GetCopyCommandQueue() };
            auto& copyCommandList = copyCommandQueue->GetCommandList(m_MaterialBuffer->GetSizeInBytes());
            copyCommandList.UpdateBuffer(*m_MaterialBuffer, std::span{ materialsData });
        }
    }

} // namespace benzin
