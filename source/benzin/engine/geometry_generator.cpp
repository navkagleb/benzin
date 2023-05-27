#include "benzin/config/bootstrap.hpp"

#include "benzin/engine/geometry_generator.hpp"

#include "benzin/engine/model.hpp"

namespace benzin::geometry_generator
{

    namespace
    {

        MeshVertexData MiddlePoint(const MeshVertexData& lhs, const MeshVertexData& rhs)
        {
            const DirectX::XMVECTOR positionLhs = DirectX::XMLoadFloat3(&lhs.LocalPosition);
            const DirectX::XMVECTOR positionRhs = DirectX::XMLoadFloat3(&rhs.LocalPosition);

            const DirectX::XMVECTOR normalLhs = DirectX::XMLoadFloat3(&lhs.LocalNormal);
            const DirectX::XMVECTOR normalRhs = DirectX::XMLoadFloat3(&rhs.LocalNormal);

            const DirectX::XMVECTOR texCoordLhs = DirectX::XMLoadFloat2(&lhs.TexCoord);
            const DirectX::XMVECTOR texCoordRhs = DirectX::XMLoadFloat2(&rhs.TexCoord);

            // Compute the midpoints of all the attributes. Vectors need to be normalized
            // since linear interpolating can make them not unit length. 
            const DirectX::XMVECTOR localPosition = DirectX::XMVectorScale(DirectX::XMVectorAdd(positionLhs, positionRhs), 0.5f);
            const DirectX::XMVECTOR localNormal = DirectX::XMVector3Normalize(DirectX::XMVectorScale(DirectX::XMVectorAdd(normalLhs, normalRhs), 0.5f));
            const DirectX::XMVECTOR texCoord = DirectX::XMVectorScale(DirectX::XMVectorAdd(texCoordLhs, texCoordRhs), 0.5f);

            MeshVertexData middle;
            DirectX::XMStoreFloat3(&middle.LocalPosition, localPosition);
            DirectX::XMStoreFloat3(&middle.LocalNormal, localNormal);
            DirectX::XMStoreFloat2(&middle.TexCoord, texCoord);

            return middle;
        }

        void Subdivide(MeshPrimitiveData& meshPrimitiveData)
        {
            MeshPrimitiveData copy = meshPrimitiveData;

            meshPrimitiveData.Vertices.clear();
            meshPrimitiveData.Indices.clear();

            //       v1
            //       *
            //      / \
            //     /   \
            //  m0*-----*m1
            //   / \   / \
            //  /   \ /   \
            // *-----*-----*
            // v0    m2     v2

            const uint32_t triangleCount = static_cast<uint32_t>(copy.Indices.size() / 3);

            for (uint32_t i = 0; i < triangleCount; ++i)
            {
                const MeshVertexData vertex0 = copy.Vertices[copy.Indices[i * 3 + 0]];
                const MeshVertexData vertex1 = copy.Vertices[copy.Indices[i * 3 + 1]];
                const MeshVertexData vertex2 = copy.Vertices[copy.Indices[i * 3 + 2]];

                const MeshVertexData middle0 = MiddlePoint(vertex0, vertex1);
                const MeshVertexData middle1 = MiddlePoint(vertex1, vertex2);
                const MeshVertexData middle2 = MiddlePoint(vertex0, vertex2);

                meshPrimitiveData.Vertices.push_back(vertex0); // 0
                meshPrimitiveData.Vertices.push_back(vertex1); // 1
                meshPrimitiveData.Vertices.push_back(vertex2); // 2
                meshPrimitiveData.Vertices.push_back(middle0); // 3
                meshPrimitiveData.Vertices.push_back(middle1); // 4
                meshPrimitiveData.Vertices.push_back(middle2); // 5

                meshPrimitiveData.Indices.push_back(i * 6 + 0);
                meshPrimitiveData.Indices.push_back(i * 6 + 3);
                meshPrimitiveData.Indices.push_back(i * 6 + 5);

                meshPrimitiveData.Indices.push_back(i * 6 + 3);
                meshPrimitiveData.Indices.push_back(i * 6 + 4);
                meshPrimitiveData.Indices.push_back(i * 6 + 5);

                meshPrimitiveData.Indices.push_back(i * 6 + 5);
                meshPrimitiveData.Indices.push_back(i * 6 + 4);
                meshPrimitiveData.Indices.push_back(i * 6 + 2);

                meshPrimitiveData.Indices.push_back(i * 6 + 3);
                meshPrimitiveData.Indices.push_back(i * 6 + 1);
                meshPrimitiveData.Indices.push_back(i * 6 + 4);
            }
        }

        void GenerateCylinderTopCap(const geometry_generator::CylinderConfig& config, MeshPrimitiveData& meshPrimitiveData)
        {
            const uint32_t baseIndex = static_cast<uint32_t>(meshPrimitiveData.Vertices.size());

            // Vertices
            {
                const float y = 0.5f * config.Height;
                const float dTheta = DirectX::XM_2PI / config.SliceCount;

                for (uint32_t i = 0; i <= config.SliceCount; ++i)
                {
                    const float x = config.TopRadius * std::cos(i * dTheta);
                    const float z = config.TopRadius * std::sin(i * dTheta);
                    const float u = x / config.Height + 0.5f;
                    const float v = z / config.Height + 0.5f;

                    meshPrimitiveData.Vertices.push_back(MeshVertexData
                    {
                        .LocalPosition{ x, y, z },
                        .LocalNormal{ 0.0f, 1.0f, 0.0f },
                        .TexCoord{ u, v }
                    });
                }

                const MeshVertexData centerVertex
                {
                    .LocalPosition{ 0.0f, y, 0.0f },
                    .LocalNormal{ 0.0f, 1.0f, 0.0f },
                    .TexCoord{ 0.5f, 0.5f }
                };

                meshPrimitiveData.Vertices.push_back(centerVertex);
            }

            // Indices
            {
                const uint32_t centerIndex = static_cast<uint32_t>(meshPrimitiveData.Vertices.size() - 1);

                for (uint32_t i = 0; i < config.SliceCount; ++i)
                {
                    meshPrimitiveData.Indices.push_back(centerIndex);
                    meshPrimitiveData.Indices.push_back(baseIndex + i + 1);
                    meshPrimitiveData.Indices.push_back(baseIndex + i);
                }
            }
        }

        void GenerateCylinderBottomCap(const geometry_generator::CylinderConfig& config, MeshPrimitiveData& meshPrimitiveData)
        {
            const uint32_t baseIndex = static_cast<uint32_t>(meshPrimitiveData.Vertices.size());

            // Vertices
            {
                const float y = -0.5f * config.Height;
                const float dTheta = DirectX::XM_2PI / config.SliceCount;

                for (uint32_t i = 0; i <= config.SliceCount; ++i)
                {
                    const float x = config.BottomRadius * cosf(i * dTheta);
                    const float z = config.BottomRadius * sinf(i * dTheta);
                    const float u = x / config.Height + 0.5f;
                    const float v = z / config.Height + 0.5f;

                    meshPrimitiveData.Vertices.push_back(MeshVertexData
                    {
                        .LocalPosition{ x, y, z },
                        .LocalNormal{ 0.0f, -1.0f, 0.0f },
                        .TexCoord{ u, v }
                    });
                }

                const MeshVertexData centerVertex
                {
                    .LocalPosition{ 0.0f, y, 0.0f },
                    .LocalNormal{ 0.0f, -1.0f, 0.0f },
                    .TexCoord{ 0.5f, 0.5f },
                };

                meshPrimitiveData.Vertices.push_back(centerVertex);
            }

            // Indices
            {
                const uint32_t centerIndex = static_cast<uint32_t>(meshPrimitiveData.Vertices.size() - 1);

                for (uint32_t i = 0; i < config.SliceCount; ++i)
                {
                    meshPrimitiveData.Indices.push_back(centerIndex);
                    meshPrimitiveData.Indices.push_back(baseIndex + i);
                    meshPrimitiveData.Indices.push_back(baseIndex + i + 1);
                }
            }
        }

    } // anonymous namespace

    MeshPrimitiveData GenerateBox(const BoxConfig& config)
    {
        MeshPrimitiveData meshPrimitiveData;
        meshPrimitiveData.PrimitiveTopology = PrimitiveTopology::TriangleList;

        const float w2 = 0.5f * config.Width;
        const float h2 = 0.5f * config.Height;
        const float d2 = 0.5f * config.Depth;

        auto& vertices = meshPrimitiveData.Vertices;
        vertices.resize(24);

        // Front face
        vertices[0] = MeshVertexData{ DirectX::XMFLOAT3{ -w2, -h2, -d2 }, DirectX::XMFLOAT3{ 0.0f, 0.0f, -1.0f }, DirectX::XMFLOAT2{ 0.0f, 1.0f } };
        vertices[1] = MeshVertexData{ DirectX::XMFLOAT3{ -w2,  h2, -d2 }, DirectX::XMFLOAT3{ 0.0f, 0.0f, -1.0f }, DirectX::XMFLOAT2{ 0.0f, 0.0f } };
        vertices[2] = MeshVertexData{ DirectX::XMFLOAT3{  w2,  h2, -d2 }, DirectX::XMFLOAT3{ 0.0f, 0.0f, -1.0f }, DirectX::XMFLOAT2{ 1.0f, 0.0f } };
        vertices[3] = MeshVertexData{ DirectX::XMFLOAT3{  w2, -h2, -d2 }, DirectX::XMFLOAT3{ 0.0f, 0.0f, -1.0f }, DirectX::XMFLOAT2{ 1.0f, 1.0f } };

        // Back face
        vertices[4] = MeshVertexData{ DirectX::XMFLOAT3{ -w2, -h2,  d2 }, DirectX::XMFLOAT3{ 0.0f, 0.0f, 1.0f }, DirectX::XMFLOAT2{ 1.0f, 1.0f } };
        vertices[5] = MeshVertexData{ DirectX::XMFLOAT3{  w2, -h2,  d2 }, DirectX::XMFLOAT3{ 0.0f, 0.0f, 1.0f }, DirectX::XMFLOAT2{ 0.0f, 1.0f } };
        vertices[6] = MeshVertexData{ DirectX::XMFLOAT3{  w2,  h2,  d2 }, DirectX::XMFLOAT3{ 0.0f, 0.0f, 1.0f }, DirectX::XMFLOAT2{ 0.0f, 0.0f } };
        vertices[7] = MeshVertexData{ DirectX::XMFLOAT3{ -w2,  h2,  d2 }, DirectX::XMFLOAT3{ 0.0f, 0.0f, 1.0f }, DirectX::XMFLOAT2{ 1.0f, 0.0f } };

        // Top face
        vertices[8] = MeshVertexData{ DirectX::XMFLOAT3{ -w2,  h2, -d2}, DirectX::XMFLOAT3{ 0.0f, 1.0f, 0.0f }, DirectX::XMFLOAT2{ 0.0f, 1.0f } };
        vertices[9] = MeshVertexData{ DirectX::XMFLOAT3{ -w2,  h2,  d2}, DirectX::XMFLOAT3{ 0.0f, 1.0f, 0.0f }, DirectX::XMFLOAT2{ 0.0f, 0.0f } };
        vertices[10] = MeshVertexData{ DirectX::XMFLOAT3{  w2,  h2,  d2}, DirectX::XMFLOAT3{ 0.0f, 1.0f, 0.0f }, DirectX::XMFLOAT2{ 1.0f, 0.0f } };
        vertices[11] = MeshVertexData{ DirectX::XMFLOAT3{  w2,  h2, -d2}, DirectX::XMFLOAT3{ 0.0f, 1.0f, 0.0f }, DirectX::XMFLOAT2{ 1.0f, 1.0f } };

        // Bottom face
        vertices[12] = MeshVertexData{ DirectX::XMFLOAT3{ -w2, -h2, -d2 }, DirectX::XMFLOAT3{ 0.0f, -1.0f, 0.0f}, DirectX::XMFLOAT2{ 1.0f, 1.0f} };
        vertices[13] = MeshVertexData{ DirectX::XMFLOAT3{  w2, -h2, -d2 }, DirectX::XMFLOAT3{ 0.0f, -1.0f, 0.0f}, DirectX::XMFLOAT2{ 0.0f, 1.0f} };
        vertices[14] = MeshVertexData{ DirectX::XMFLOAT3{  w2, -h2,  d2 }, DirectX::XMFLOAT3{ 0.0f, -1.0f, 0.0f}, DirectX::XMFLOAT2{ 0.0f, 0.0f} };
        vertices[15] = MeshVertexData{ DirectX::XMFLOAT3{ -w2, -h2,  d2 }, DirectX::XMFLOAT3{ 0.0f, -1.0f, 0.0f}, DirectX::XMFLOAT2{ 1.0f, 0.0f} };

        // Left face
        vertices[16] = MeshVertexData{ DirectX::XMFLOAT3{ -w2, -h2,  d2 }, DirectX::XMFLOAT3{ -1.0f, 0.0f, 0.0f }, DirectX::XMFLOAT2{ 0.0f, 1.0f } };
        vertices[17] = MeshVertexData{ DirectX::XMFLOAT3{ -w2,  h2,  d2 }, DirectX::XMFLOAT3{ -1.0f, 0.0f, 0.0f }, DirectX::XMFLOAT2{ 0.0f, 0.0f } };
        vertices[18] = MeshVertexData{ DirectX::XMFLOAT3{ -w2,  h2, -d2 }, DirectX::XMFLOAT3{ -1.0f, 0.0f, 0.0f }, DirectX::XMFLOAT2{ 1.0f, 0.0f } };
        vertices[19] = MeshVertexData{ DirectX::XMFLOAT3{ -w2, -h2, -d2 }, DirectX::XMFLOAT3{ -1.0f, 0.0f, 0.0f }, DirectX::XMFLOAT2{ 1.0f, 1.0f } };

        // Right face
        vertices[20] = MeshVertexData{ DirectX::XMFLOAT3{  w2, -h2, -d2 }, DirectX::XMFLOAT3{ 1.0f, 0.0f, 0.0f }, DirectX::XMFLOAT2{ 0.0f, 1.0f } };
        vertices[21] = MeshVertexData{ DirectX::XMFLOAT3{  w2,  h2, -d2 }, DirectX::XMFLOAT3{ 1.0f, 0.0f, 0.0f }, DirectX::XMFLOAT2{ 0.0f, 0.0f } };
        vertices[22] = MeshVertexData{ DirectX::XMFLOAT3{  w2,  h2,  d2 }, DirectX::XMFLOAT3{ 1.0f, 0.0f, 0.0f }, DirectX::XMFLOAT2{ 1.0f, 0.0f } };
        vertices[23] = MeshVertexData{ DirectX::XMFLOAT3{  w2, -h2,  d2 }, DirectX::XMFLOAT3{ 1.0f, 0.0f, 0.0f }, DirectX::XMFLOAT2{ 1.0f, 1.0f } };

        // Indices
        meshPrimitiveData.Indices =
        {
            // Front face
            0, 1, 2,
            0, 2, 3,

            // Back face
            4, 5, 6,
            4, 6, 7,

            // Top face
            8, 9,  10,
            8, 10, 11,

            // Bottom face
            12, 13, 14,
            12, 14, 15,

            // Left face
            16, 17, 18,
            16, 18, 19,

            // Right face
            20, 21, 22,
            20, 22, 23
        };

        const uint32_t subdivisionCount = std::min<uint32_t>(config.SubdivisionCount, 6);

        for (uint32_t i = 0; i < subdivisionCount; ++i)
        {
            Subdivide(meshPrimitiveData);
        }

        return meshPrimitiveData;
    }

    MeshPrimitiveData GenerateGrid(const GridConfig& config)
    {
        MeshPrimitiveData meshPrimitiveData;
        meshPrimitiveData.PrimitiveTopology = PrimitiveTopology::TriangleList;

        const uint32_t vertexCount = config.RowCount * config.ColumnCount;
        const uint32_t faceCount = (config.RowCount - 1) * (config.ColumnCount - 1) * 2;

        // Vertices
        {
            const float halfWidth = 0.5f * config.Width;
            const float halfDepth = 0.5f * config.Depth;

            const float dx = config.Width / (config.RowCount - 1);
            const float dz = config.Depth / (config.ColumnCount - 1);

            const float du = 1.0f / (config.RowCount - 1);
            const float dv = 1.0f / (config.ColumnCount - 1);

            auto& vertices = meshPrimitiveData.Vertices;
            vertices.resize(vertexCount);

            for (uint32_t i = 0; i < config.RowCount; ++i)
            {
                const float z = halfDepth - i * dz;

                for (uint32_t j = 0; j < config.ColumnCount; ++j)
                {
                    const float x = -halfWidth + j * dx;

                    vertices[i * config.ColumnCount + j].LocalPosition = DirectX::XMFLOAT3{ x, 0.0f, z };
                    vertices[i * config.ColumnCount + j].LocalNormal = DirectX::XMFLOAT3{ 0.0f, 1.0f, 0.0f };
                    vertices[i * config.ColumnCount + j].TexCoord = DirectX::XMFLOAT2{ j * du, i * dv };
                }
            }
        }

        // Indices
        {
            auto& indices = meshPrimitiveData.Indices;
            indices.resize(faceCount * 3);

            size_t baseIndex = 0;

            for (uint32_t i = 0; i < config.RowCount - 1; ++i)
            {
                for (uint32_t j = 0; j < config.ColumnCount - 1; ++j)
                {
                    indices[baseIndex + 0] = i * config.ColumnCount + j;
                    indices[baseIndex + 1] = i * config.ColumnCount + j + 1;
                    indices[baseIndex + 2] = (i + 1) * config.ColumnCount + j;

                    indices[baseIndex + 3] = (i + 1) * config.ColumnCount + j;
                    indices[baseIndex + 4] = i * config.ColumnCount + j + 1;
                    indices[baseIndex + 5] = (i + 1) * config.ColumnCount + j + 1;

                    baseIndex += 6;
                }
            }
        }

        return meshPrimitiveData;
    }

    MeshPrimitiveData GenerateCylinder(const CylinderConfig& config)
    {
        MeshPrimitiveData meshPrimitiveData;
        meshPrimitiveData.PrimitiveTopology = PrimitiveTopology::TriangleList;

        // Vertices
        {
            const float stackHeight = config.Height / static_cast<float>(config.StackCount);
            const float radiusStep = (config.TopRadius - config.BottomRadius) / static_cast<float>(config.StackCount);

            const uint32_t ringCount = config.StackCount + 1;

            for (uint32_t i = 0; i < ringCount; ++i)
            {
                const float y = -0.5f * config.Height + static_cast<float>(i) * stackHeight;
                const float r = config.BottomRadius + static_cast<float>(i) * radiusStep;

                const float dTheta = DirectX::XM_2PI / static_cast<float>(config.SliceCount);

                // Ring Vertices
                for (uint32_t j = 0; j <= config.SliceCount; ++j)
                {
                    MeshVertexData& vertex = meshPrimitiveData.Vertices.emplace_back();

                    const float c = std::cos(j * dTheta);
                    const float s = std::sin(j * dTheta);

                    vertex.LocalPosition = DirectX::XMFLOAT3{ r * c, y, r * s };

                    vertex.TexCoord.x = static_cast<float>(j) / config.SliceCount;
                    vertex.TexCoord.y = 1.0f - static_cast<float>(i) / config.StackCount;

                    const DirectX::XMFLOAT3 tangent{ -s, 0.0f, c };

                    const float dr = config.BottomRadius - config.TopRadius;
                    const DirectX::XMFLOAT3 bitangent{ dr * c, -config.Height, dr * s };

                    const DirectX::XMVECTOR t = DirectX::XMLoadFloat3(&tangent);
                    const DirectX::XMVECTOR b = DirectX::XMLoadFloat3(&bitangent);
                    const DirectX::XMVECTOR n = DirectX::XMVector3Normalize(DirectX::XMVector3Cross(t, b));

                    DirectX::XMStoreFloat3(&vertex.LocalNormal, n);
                }
            }
        }

        // Indices
        {
            const uint32_t ringVertexCount = config.SliceCount + 1;

            for (uint32_t i = 0; i < config.StackCount; ++i)
            {
                for (uint32_t j = 0; j < config.SliceCount; ++j)
                {
                    meshPrimitiveData.Indices.push_back(i * ringVertexCount + j);
                    meshPrimitiveData.Indices.push_back((i + 1) * ringVertexCount + j);
                    meshPrimitiveData.Indices.push_back((i + 1) * ringVertexCount + j + 1);

                    meshPrimitiveData.Indices.push_back(i * ringVertexCount + j);
                    meshPrimitiveData.Indices.push_back((i + 1) * ringVertexCount + j + 1);
                    meshPrimitiveData.Indices.push_back(i * ringVertexCount + j + 1);
                }
            }
        }

        GenerateCylinderTopCap(config, meshPrimitiveData);
        GenerateCylinderBottomCap(config, meshPrimitiveData);

        return meshPrimitiveData;
    }

    MeshPrimitiveData GenerateSphere(const SphereConfig& config)
    {
        MeshPrimitiveData meshPrimitiveData;
        meshPrimitiveData.PrimitiveTopology = PrimitiveTopology::TriangleList;

        // Vertices
        {
            const MeshVertexData topVertex
            {
                .LocalPosition{ 0.0f, config.Radius, 0.0f },
                .LocalNormal{ 0.0f, 1.0f, 0.0f },
                .TexCoord{ 0.0f, 0.0f }
            };

            const MeshVertexData bottomVertex
            {
                .LocalPosition{ 0.0f, -config.Radius, 0.0f },
                .LocalNormal{ 0.0f, -1.0f, 0.0f },
                .TexCoord{ 0.0f, 1.0f },
            };

            meshPrimitiveData.Vertices.push_back(topVertex);

            const float phiStep = DirectX::XM_PI / static_cast<float>(config.StackCount);
            const float thetaStep = DirectX::XM_2PI / static_cast<float>(config.SliceCount);

            for (uint32_t i = 1; i < config.StackCount; ++i)
            {
                const float phi = i * phiStep;

                for (uint32_t j = 0; j <= config.SliceCount; ++j)
                {
                    const float theta = j * thetaStep;

                    MeshVertexData& vertex = meshPrimitiveData.Vertices.emplace_back();

                    // Spherical to cartesian
                    vertex.LocalPosition.x = config.Radius * std::sin(phi) * std::cos(theta);
                    vertex.LocalPosition.y = config.Radius * std::cos(phi);
                    vertex.LocalPosition.z = config.Radius * std::sin(phi) * std::sin(theta);

                    DirectX::XMStoreFloat3(&vertex.LocalNormal, DirectX::XMVector3Normalize(DirectX::XMLoadFloat3(&vertex.LocalPosition)));

                    vertex.TexCoord.x = theta / DirectX::XM_2PI;
                    vertex.TexCoord.y = phi / DirectX::XM_PI;
                }
            }

            meshPrimitiveData.Vertices.push_back(bottomVertex);
        }

        // Indices
        {
            for (uint32_t i = 1; i <= config.SliceCount; ++i)
            {
                meshPrimitiveData.Indices.push_back(0);
                meshPrimitiveData.Indices.push_back(i + 1);
                meshPrimitiveData.Indices.push_back(i);
            }

            const uint32_t ringVertexCount = config.SliceCount + 1;

            uint32_t baseIndex = 1;

            for (uint32_t i = 0; i < config.StackCount - 2; ++i)
            {
                for (uint32_t j = 0; j < config.SliceCount; ++j)
                {
                    meshPrimitiveData.Indices.push_back(baseIndex + i * ringVertexCount + j);
                    meshPrimitiveData.Indices.push_back(baseIndex + i * ringVertexCount + j + 1);
                    meshPrimitiveData.Indices.push_back(baseIndex + (i + 1) * ringVertexCount + j);

                    meshPrimitiveData.Indices.push_back(baseIndex + (i + 1) * ringVertexCount + j);
                    meshPrimitiveData.Indices.push_back(baseIndex + i * ringVertexCount + j + 1);
                    meshPrimitiveData.Indices.push_back(baseIndex + (i + 1) * ringVertexCount + j + 1);
                }
            }

            const uint32_t southPoleIndex = static_cast<uint32_t>(meshPrimitiveData.Vertices.size() - 1);

            baseIndex = southPoleIndex - ringVertexCount;

            for (uint32_t i = 0; i < config.SliceCount; ++i)
            {
                meshPrimitiveData.Indices.push_back(southPoleIndex);
                meshPrimitiveData.Indices.push_back(baseIndex + i);
                meshPrimitiveData.Indices.push_back(baseIndex + i + 1);
            }
        }

        return meshPrimitiveData;
    }

    MeshPrimitiveData GenerateGeosphere(const GeosphereConfig& config)
    {
        static constexpr float x = 0.525731f;
        static constexpr float z = 0.850651f;

        static constexpr std::array<DirectX::XMFLOAT3, 12> positions
        {
            DirectX::XMFLOAT3{ -x,     0.0f,  z    }, DirectX::XMFLOAT3{  x,     0.0f,  z    },
            DirectX::XMFLOAT3{ -x,     0.0f, -z    }, DirectX::XMFLOAT3{  x,     0.0f, -z    },
            DirectX::XMFLOAT3{  0.0f,  z,     x    }, DirectX::XMFLOAT3{  0.0f,  z,    -x    },
            DirectX::XMFLOAT3{  0.0f, -z,     x    }, DirectX::XMFLOAT3{  0.0f, -z,    -x    },
            DirectX::XMFLOAT3{  z,     x,     0.0f }, DirectX::XMFLOAT3{ -z,     x,     0.0f },
            DirectX::XMFLOAT3{  z,    -x,     0.0f }, DirectX::XMFLOAT3{ -z,    -x,     0.0f }
        };

        static constexpr std::array<uint32_t, 60> indicies
        {
            1,  4,  0,      4,  9, 0,       4, 5,  9,       8, 5, 4,        1,  8, 4,
            1,  10, 8,      10, 3, 8,       8, 3,  5,       3, 2, 5,        3,  7, 2,
            3,  10, 7,      10, 6, 7,       6, 11, 7,       6, 0, 11,       6,  1, 0,
            10, 1,  6,      11, 0, 9,       2, 11, 9,       5, 2, 9,        11, 2, 7
        };

        MeshPrimitiveData meshPrimitiveData;
        meshPrimitiveData.PrimitiveTopology = PrimitiveTopology::TriangleList;
        meshPrimitiveData.Vertices.insert(meshPrimitiveData.Vertices.begin(), positions.begin(), positions.end());
        meshPrimitiveData.Indices.insert(meshPrimitiveData.Indices.begin(), indicies.begin(), indicies.end());
        
        const uint32_t subdivisionCount = std::min<uint32_t>(config.SubdivisionCount, 6);
        for (uint32_t i = 0; i < subdivisionCount; ++i)
        {
            Subdivide(meshPrimitiveData);
        }

        for (uint32_t i = 0; i < meshPrimitiveData.Vertices.size(); ++i)
        {
            const DirectX::XMVECTOR normal = DirectX::XMVector3Normalize(DirectX::XMLoadFloat3(&meshPrimitiveData.Vertices[i].LocalPosition));
            const DirectX::XMVECTOR position = DirectX::XMVectorScale(normal, config.Radius);

            DirectX::XMStoreFloat3(&meshPrimitiveData.Vertices[i].LocalPosition, position);
            DirectX::XMStoreFloat3(&meshPrimitiveData.Vertices[i].LocalNormal, normal);

            float theta = std::atan2(meshPrimitiveData.Vertices[i].LocalPosition.z, meshPrimitiveData.Vertices[i].LocalPosition.x);

            if (theta < 0.0f)
            {
                theta += DirectX::XM_2PI;
            }

            const float phi = std::acosf(meshPrimitiveData.Vertices[i].LocalPosition.y / config.Radius);

            meshPrimitiveData.Vertices[i].TexCoord.x = theta / DirectX::XM_2PI;
            meshPrimitiveData.Vertices[i].TexCoord.y = phi / DirectX::XM_PI;
        }

        return meshPrimitiveData;
    }

} // namespace benzin::geometry_generator
