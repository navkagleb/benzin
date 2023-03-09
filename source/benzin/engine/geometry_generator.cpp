#include "benzin/config/bootstrap.hpp"

#include "benzin/engine/geometry_generator.hpp"

namespace benzin::geometry_generator
{

    namespace
    {

        Vertex MiddlePoint(const Vertex& lhs, const Vertex& rhs)
        {
            const DirectX::XMVECTOR positionLhs = DirectX::XMLoadFloat3(&lhs.Position);
            const DirectX::XMVECTOR positionRhs = DirectX::XMLoadFloat3(&rhs.Position);

            const DirectX::XMVECTOR normalLhs = DirectX::XMLoadFloat3(&lhs.Normal);
            const DirectX::XMVECTOR normalRhs = DirectX::XMLoadFloat3(&rhs.Normal);

            const DirectX::XMVECTOR tangentLhs = DirectX::XMLoadFloat3(&lhs.Tangent);
            const DirectX::XMVECTOR tangentRhs = DirectX::XMLoadFloat3(&rhs.Tangent);

            const DirectX::XMVECTOR texCoordLhs = DirectX::XMLoadFloat2(&lhs.TexCoord);
            const DirectX::XMVECTOR texCoordRhs = DirectX::XMLoadFloat2(&rhs.TexCoord);

            // Compute the midpoints of all the attributes. Vectors need to be normalized
            // since linear interpolating can make them not unit length. 
            const DirectX::XMVECTOR position = DirectX::XMVectorScale(DirectX::XMVectorAdd(positionLhs, positionRhs), 0.5f);
            const DirectX::XMVECTOR normal = DirectX::XMVector3Normalize(DirectX::XMVectorScale(DirectX::XMVectorAdd(normalLhs, normalRhs), 0.5f));
            const DirectX::XMVECTOR tangent = DirectX::XMVector3Normalize(DirectX::XMVectorScale(DirectX::XMVectorAdd(tangentLhs, tangentRhs), 0.5f));
            const DirectX::XMVECTOR texCoord = DirectX::XMVectorScale(DirectX::XMVectorAdd(texCoordLhs, texCoordRhs), 0.5f);

            Vertex middle;
            DirectX::XMStoreFloat3(&middle.Position, position);
            DirectX::XMStoreFloat3(&middle.Normal, normal);
            DirectX::XMStoreFloat3(&middle.Tangent, tangent);
            DirectX::XMStoreFloat2(&middle.TexCoord, texCoord);

            return middle;
        }

        void Subdivide(MeshData& meshData)
        {
            MeshData copy = meshData;

            meshData.Vertices.clear();
            meshData.Indices.clear();

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
                const Vertex v0 = copy.Vertices[copy.Indices[i * 3 + 0]];
                const Vertex v1 = copy.Vertices[copy.Indices[i * 3 + 1]];
                const Vertex v2 = copy.Vertices[copy.Indices[i * 3 + 2]];

                const Vertex middle0 = MiddlePoint(v0, v1);
                const Vertex middle1 = MiddlePoint(v1, v2);
                const Vertex middle2 = MiddlePoint(v0, v2);

                meshData.Vertices.push_back(v0);        // 0
                meshData.Vertices.push_back(v1);        // 1
                meshData.Vertices.push_back(v2);        // 2
                meshData.Vertices.push_back(middle0);   // 3
                meshData.Vertices.push_back(middle1);   // 4
                meshData.Vertices.push_back(middle2);   // 5

                meshData.Indices.push_back(i * 6 + 0);
                meshData.Indices.push_back(i * 6 + 3);
                meshData.Indices.push_back(i * 6 + 5);

                meshData.Indices.push_back(i * 6 + 3);
                meshData.Indices.push_back(i * 6 + 4);
                meshData.Indices.push_back(i * 6 + 5);

                meshData.Indices.push_back(i * 6 + 5);
                meshData.Indices.push_back(i * 6 + 4);
                meshData.Indices.push_back(i * 6 + 2);

                meshData.Indices.push_back(i * 6 + 3);
                meshData.Indices.push_back(i * 6 + 1);
                meshData.Indices.push_back(i * 6 + 4);
            }
        }

        void GenerateCylinderTopCap(const geometry_generator::CylinderConfig& config, MeshData& meshData)
        {
            const uint32_t baseIndex = static_cast<uint32_t>(meshData.Vertices.size());

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

                    meshData.Vertices.push_back(Vertex
                    {
                        .Position{ x, y, z },
                        .Normal{ 0.0f, 1.0f, 0.0f },
                        .Tangent{ 1.0f, 0.0f, 0.0f },
                        .TexCoord{ u, v }
                    });
                }

                const Vertex centerVertex
                {
                    .Position{ 0.0f, y, 0.0f },
                    .Normal{ 0.0f, 1.0f, 0.0f },
                    .Tangent{ 1.0f, 0.0f, 0.0f },
                    .TexCoord{ 0.5f, 0.5f }
                };

                meshData.Vertices.push_back(centerVertex);
            }

            // Indices
            {
                const uint32_t centerIndex = static_cast<uint32_t>(meshData.Vertices.size() - 1);

                for (uint32_t i = 0; i < config.SliceCount; ++i)
                {
                    meshData.Indices.push_back(centerIndex);
                    meshData.Indices.push_back(baseIndex + i + 1);
                    meshData.Indices.push_back(baseIndex + i);
                }
            }
        }

        void GenerateCylinderBottomCap(const geometry_generator::CylinderConfig& config, MeshData& meshData)
        {
            const uint32_t baseIndex = static_cast<uint32_t>(meshData.Vertices.size());

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

                    meshData.Vertices.push_back(Vertex
                    {
                        .Position{ x, y, z },
                        .Normal{ 0.0f, -1.0f, 0.0f },
                        .Tangent{ 1.0f, 0.0f, 0.0f },
                        .TexCoord{ u, v }
                    });
                }

                const Vertex centerVertex
                {
                    .Position{ 0.0f, y, 0.0f },
                    .Normal{ 0.0f, -1.0f, 0.0f },
                    .Tangent{ 1.0f, 0.0f, 0.0f },
                    .TexCoord{ 0.5f, 0.5f },
                };

                meshData.Vertices.push_back(centerVertex);
            }

            // Indices
            {
                const uint32_t centerIndex = static_cast<uint32_t>(meshData.Vertices.size() - 1);

                for (uint32_t i = 0; i < config.SliceCount; ++i)
                {
                    meshData.Indices.push_back(centerIndex);
                    meshData.Indices.push_back(baseIndex + i);
                    meshData.Indices.push_back(baseIndex + i + 1);
                }
            }
        }

    } // anonymous namespace

    MeshData GenerateBox(const BoxConfig& config)
    {
        MeshData meshData;

        const float w2 = 0.5f * config.Width;
        const float h2 = 0.5f * config.Height;
        const float d2 = 0.5f * config.Depth;

        auto& vertices = meshData.Vertices;
        vertices.resize(24);

        // Front face
        vertices[0] = Vertex{ DirectX::XMFLOAT3{ -w2, -h2, -d2 }, DirectX::XMFLOAT3{ 0.0f, 0.0f, -1.0f }, DirectX::XMFLOAT3{ 1.0f, 0.0f, 0.0f }, DirectX::XMFLOAT2{ 0.0f, 1.0f } };
        vertices[1] = Vertex{ DirectX::XMFLOAT3{ -w2,  h2, -d2 }, DirectX::XMFLOAT3{ 0.0f, 0.0f, -1.0f }, DirectX::XMFLOAT3{ 1.0f, 0.0f, 0.0f }, DirectX::XMFLOAT2{ 0.0f, 0.0f } };
        vertices[2] = Vertex{ DirectX::XMFLOAT3{  w2,  h2, -d2 }, DirectX::XMFLOAT3{ 0.0f, 0.0f, -1.0f }, DirectX::XMFLOAT3{ 1.0f, 0.0f, 0.0f }, DirectX::XMFLOAT2{ 1.0f, 0.0f } };
        vertices[3] = Vertex{ DirectX::XMFLOAT3{  w2, -h2, -d2 }, DirectX::XMFLOAT3{ 0.0f, 0.0f, -1.0f }, DirectX::XMFLOAT3{ 1.0f, 0.0f, 0.0f }, DirectX::XMFLOAT2{ 1.0f, 1.0f } };

        // Back face
        vertices[4] = Vertex{ DirectX::XMFLOAT3{ -w2, -h2,  d2 }, DirectX::XMFLOAT3{ 0.0f, 0.0f, 1.0f }, DirectX::XMFLOAT3{ -1.0f, 0.0f, 0.0f }, DirectX::XMFLOAT2{ 1.0f, 1.0f } };
        vertices[5] = Vertex{ DirectX::XMFLOAT3{  w2, -h2,  d2 }, DirectX::XMFLOAT3{ 0.0f, 0.0f, 1.0f }, DirectX::XMFLOAT3{ -1.0f, 0.0f, 0.0f }, DirectX::XMFLOAT2{ 0.0f, 1.0f } };
        vertices[6] = Vertex{ DirectX::XMFLOAT3{  w2,  h2,  d2 }, DirectX::XMFLOAT3{ 0.0f, 0.0f, 1.0f }, DirectX::XMFLOAT3{ -1.0f, 0.0f, 0.0f }, DirectX::XMFLOAT2{ 0.0f, 0.0f } };
        vertices[7] = Vertex{ DirectX::XMFLOAT3{ -w2,  h2,  d2 }, DirectX::XMFLOAT3{ 0.0f, 0.0f, 1.0f }, DirectX::XMFLOAT3{ -1.0f, 0.0f, 0.0f }, DirectX::XMFLOAT2{ 1.0f, 0.0f } };

        // Top face
        vertices[8] = Vertex{ DirectX::XMFLOAT3{ -w2,  h2, -d2}, DirectX::XMFLOAT3{ 0.0f, 1.0f, 0.0f }, DirectX::XMFLOAT3{ 1.0f, 0.0f, 0.0f }, DirectX::XMFLOAT2{ 0.0f, 1.0f } };
        vertices[9] = Vertex{ DirectX::XMFLOAT3{ -w2,  h2,  d2}, DirectX::XMFLOAT3{ 0.0f, 1.0f, 0.0f }, DirectX::XMFLOAT3{ 1.0f, 0.0f, 0.0f }, DirectX::XMFLOAT2{ 0.0f, 0.0f } };
        vertices[10] = Vertex{ DirectX::XMFLOAT3{  w2,  h2,  d2}, DirectX::XMFLOAT3{ 0.0f, 1.0f, 0.0f }, DirectX::XMFLOAT3{ 1.0f, 0.0f, 0.0f }, DirectX::XMFLOAT2{ 1.0f, 0.0f } };
        vertices[11] = Vertex{ DirectX::XMFLOAT3{  w2,  h2, -d2}, DirectX::XMFLOAT3{ 0.0f, 1.0f, 0.0f }, DirectX::XMFLOAT3{ 1.0f, 0.0f, 0.0f }, DirectX::XMFLOAT2{ 1.0f, 1.0f } };

        // Bottom face
        vertices[12] = Vertex{ DirectX::XMFLOAT3{ -w2, -h2, -d2 }, DirectX::XMFLOAT3{ 0.0f, -1.0f, 0.0f}, DirectX::XMFLOAT3{ -1.0f, 0.0f, 0.0f}, DirectX::XMFLOAT2{ 1.0f, 1.0f} };
        vertices[13] = Vertex{ DirectX::XMFLOAT3{  w2, -h2, -d2 }, DirectX::XMFLOAT3{ 0.0f, -1.0f, 0.0f}, DirectX::XMFLOAT3{ -1.0f, 0.0f, 0.0f}, DirectX::XMFLOAT2{ 0.0f, 1.0f} };
        vertices[14] = Vertex{ DirectX::XMFLOAT3{  w2, -h2,  d2 }, DirectX::XMFLOAT3{ 0.0f, -1.0f, 0.0f}, DirectX::XMFLOAT3{ -1.0f, 0.0f, 0.0f}, DirectX::XMFLOAT2{ 0.0f, 0.0f} };
        vertices[15] = Vertex{ DirectX::XMFLOAT3{ -w2, -h2,  d2 }, DirectX::XMFLOAT3{ 0.0f, -1.0f, 0.0f}, DirectX::XMFLOAT3{ -1.0f, 0.0f, 0.0f}, DirectX::XMFLOAT2{ 1.0f, 0.0f} };

        // Left face
        vertices[16] = Vertex{ DirectX::XMFLOAT3{ -w2, -h2,  d2 }, DirectX::XMFLOAT3{ -1.0f, 0.0f, 0.0f }, DirectX::XMFLOAT3{ 0.0f, 0.0f, -1.0f}, DirectX::XMFLOAT2{ 0.0f, 1.0f } };
        vertices[17] = Vertex{ DirectX::XMFLOAT3{ -w2,  h2,  d2 }, DirectX::XMFLOAT3{ -1.0f, 0.0f, 0.0f }, DirectX::XMFLOAT3{ 0.0f, 0.0f, -1.0f}, DirectX::XMFLOAT2{ 0.0f, 0.0f } };
        vertices[18] = Vertex{ DirectX::XMFLOAT3{ -w2,  h2, -d2 }, DirectX::XMFLOAT3{ -1.0f, 0.0f, 0.0f }, DirectX::XMFLOAT3{ 0.0f, 0.0f, -1.0f}, DirectX::XMFLOAT2{ 1.0f, 0.0f } };
        vertices[19] = Vertex{ DirectX::XMFLOAT3{ -w2, -h2, -d2 }, DirectX::XMFLOAT3{ -1.0f, 0.0f, 0.0f }, DirectX::XMFLOAT3{ 0.0f, 0.0f, -1.0f}, DirectX::XMFLOAT2{ 1.0f, 1.0f } };

        // Right face
        vertices[20] = Vertex{ DirectX::XMFLOAT3{  w2, -h2, -d2 }, DirectX::XMFLOAT3{ 1.0f, 0.0f, 0.0f }, DirectX::XMFLOAT3{ 0.0f, 0.0f, 1.0f }, DirectX::XMFLOAT2{ 0.0f, 1.0f } };
        vertices[21] = Vertex{ DirectX::XMFLOAT3{  w2,  h2, -d2 }, DirectX::XMFLOAT3{ 1.0f, 0.0f, 0.0f }, DirectX::XMFLOAT3{ 0.0f, 0.0f, 1.0f }, DirectX::XMFLOAT2{ 0.0f, 0.0f } };
        vertices[22] = Vertex{ DirectX::XMFLOAT3{  w2,  h2,  d2 }, DirectX::XMFLOAT3{ 1.0f, 0.0f, 0.0f }, DirectX::XMFLOAT3{ 0.0f, 0.0f, 1.0f }, DirectX::XMFLOAT2{ 1.0f, 0.0f } };
        vertices[23] = Vertex{ DirectX::XMFLOAT3{  w2, -h2,  d2 }, DirectX::XMFLOAT3{ 1.0f, 0.0f, 0.0f }, DirectX::XMFLOAT3{ 0.0f, 0.0f, 1.0f }, DirectX::XMFLOAT2{ 1.0f, 1.0f } };

        // Indices
        meshData.Indices =
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
            Subdivide(meshData);
        }

        return meshData;
    }

    MeshData GenerateGrid(const GridConfig& config)
    {
        MeshData meshData;

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

            auto& vertices = meshData.Vertices;
            vertices.resize(vertexCount);

            for (uint32_t i = 0; i < config.RowCount; ++i)
            {
                const float z = halfDepth - i * dz;

                for (uint32_t j = 0; j < config.ColumnCount; ++j)
                {
                    const float x = -halfWidth + j * dx;

                    vertices[i * config.ColumnCount + j].Position = DirectX::XMFLOAT3{ x, 0.0f, z };
                    vertices[i * config.ColumnCount + j].Normal = DirectX::XMFLOAT3{ 0.0f, 1.0f, 0.0f };
                    vertices[i * config.ColumnCount + j].Tangent = DirectX::XMFLOAT3{ 1.0f, 0.0f, 0.0f };
                    vertices[i * config.ColumnCount + j].TexCoord = DirectX::XMFLOAT2{ j * du, i * dv };
                }
            }
        }

        // Indices
        {
            auto& indices = meshData.Indices;
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

        return meshData;
    }

    MeshData GenerateCylinder(const CylinderConfig& config)
    {
        MeshData meshData;

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
                    Vertex vertex;

                    const float c = std::cos(j * dTheta);
                    const float s = std::sin(j * dTheta);

                    vertex.Position = DirectX::XMFLOAT3{ r * c, y, r * s };

                    vertex.TexCoord.x = static_cast<float>(j) / config.SliceCount;
                    vertex.TexCoord.y = 1.0f - static_cast<float>(i) / config.StackCount;

                    vertex.Tangent = DirectX::XMFLOAT3{ -s, 0.0f, c };

                    const float dr = config.BottomRadius - config.TopRadius;
                    const DirectX::XMFLOAT3 bitangent{ dr * c, -config.Height, dr * s };

                    const DirectX::XMVECTOR t = DirectX::XMLoadFloat3(&vertex.Tangent);
                    const DirectX::XMVECTOR b = DirectX::XMLoadFloat3(&bitangent);
                    const DirectX::XMVECTOR n = DirectX::XMVector3Normalize(DirectX::XMVector3Cross(t, b));

                    DirectX::XMStoreFloat3(&vertex.Normal, n);

                    meshData.Vertices.push_back(vertex);
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
                    meshData.Indices.push_back(i * ringVertexCount + j);
                    meshData.Indices.push_back((i + 1) * ringVertexCount + j);
                    meshData.Indices.push_back((i + 1) * ringVertexCount + j + 1);

                    meshData.Indices.push_back(i * ringVertexCount + j);
                    meshData.Indices.push_back((i + 1) * ringVertexCount + j + 1);
                    meshData.Indices.push_back(i * ringVertexCount + j + 1);
                }
            }
        }

        GenerateCylinderTopCap(config, meshData);
        GenerateCylinderBottomCap(config, meshData);

        return meshData;
    }

    MeshData GenerateSphere(const SphereConfig& config)
    {
        MeshData meshData;

        // Vertices
        {
            const Vertex topVertex
            {
                .Position{ 0.0f, config.Radius, 0.0f },
                .Normal{ 0.0f, 1.0f, 0.0f },
                .Tangent{ 1.0f, 0.0f, 0.0f },
                .TexCoord{ 0.0f, 0.0f }
            };

            const Vertex bottomVertex
            {
                .Position{ 0.0f, -config.Radius, 0.0f },
                .Normal{ 0.0f, -1.0f, 0.0f },
                .Tangent{ 1.0f, 0.0f, 0.0f },
                .TexCoord{ 0.0f, 1.0f },
            };

            meshData.Vertices.push_back(topVertex);

            const float phiStep = DirectX::XM_PI / static_cast<float>(config.StackCount);
            const float thetaStep = DirectX::XM_2PI / static_cast<float>(config.SliceCount);

            for (uint32_t i = 1; i < config.StackCount; ++i)
            {
                const float phi = i * phiStep;

                for (uint32_t j = 0; j <= config.SliceCount; ++j)
                {
                    const float theta = j * thetaStep;

                    Vertex vertex;

                    // Spherical to cartesian
                    vertex.Position.x = config.Radius * std::sin(phi) * std::cos(theta);
                    vertex.Position.y = config.Radius * std::cos(phi);
                    vertex.Position.z = config.Radius * std::sin(phi) * std::sin(theta);

                    // Partial derivative of P with respect to theta
                    vertex.Tangent.x = -config.Radius * std::sin(phi) * std::sin(theta);
                    vertex.Tangent.y = 0.0f;
                    vertex.Tangent.z = config.Radius * std::sin(phi) * std::cos(theta);

                    DirectX::XMStoreFloat3(&vertex.Tangent, DirectX::XMVector3Normalize(DirectX::XMLoadFloat3(&vertex.Tangent)));
                    DirectX::XMStoreFloat3(&vertex.Normal, DirectX::XMVector3Normalize(DirectX::XMLoadFloat3(&vertex.Position)));

                    vertex.TexCoord.x = theta / DirectX::XM_2PI;
                    vertex.TexCoord.y = phi / DirectX::XM_PI;

                    meshData.Vertices.push_back(vertex);
                }
            }

            meshData.Vertices.push_back(bottomVertex);
        }

        // Indices
        {
            for (uint32_t i = 1; i <= config.SliceCount; ++i)
            {
                meshData.Indices.push_back(0);
                meshData.Indices.push_back(i + 1);
                meshData.Indices.push_back(i);
            }

            const uint32_t ringVertexCount = config.SliceCount + 1;

            uint32_t baseIndex = 1;

            for (uint32_t i = 0; i < config.StackCount - 2; ++i)
            {
                for (uint32_t j = 0; j < config.SliceCount; ++j)
                {
                    meshData.Indices.push_back(baseIndex + i * ringVertexCount + j);
                    meshData.Indices.push_back(baseIndex + i * ringVertexCount + j + 1);
                    meshData.Indices.push_back(baseIndex + (i + 1) * ringVertexCount + j);

                    meshData.Indices.push_back(baseIndex + (i + 1) * ringVertexCount + j);
                    meshData.Indices.push_back(baseIndex + i * ringVertexCount + j + 1);
                    meshData.Indices.push_back(baseIndex + (i + 1) * ringVertexCount + j + 1);
                }
            }

            const uint32_t southPoleIndex = static_cast<uint32_t>(meshData.Vertices.size() - 1);

            baseIndex = southPoleIndex - ringVertexCount;

            for (uint32_t i = 0; i < config.SliceCount; ++i)
            {
                meshData.Indices.push_back(southPoleIndex);
                meshData.Indices.push_back(baseIndex + i);
                meshData.Indices.push_back(baseIndex + i + 1);
            }
        }

        return meshData;
    }

    MeshData GenerateGeosphere(const GeosphereConfig& config)
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

        MeshData meshData;
        meshData.Vertices.insert(meshData.Vertices.begin(), positions.begin(), positions.end());
        meshData.Indices.insert(meshData.Indices.begin(), indicies.begin(), indicies.end());
        
        const uint32_t subdivisionCount = std::min<uint32_t>(config.SubdivisionCount, 6);
        for (uint32_t i = 0; i < subdivisionCount; ++i)
        {
            Subdivide(meshData);
        }

        for (uint32_t i = 0; i < meshData.Vertices.size(); ++i)
        {
            const DirectX::XMVECTOR normal = DirectX::XMVector3Normalize(DirectX::XMLoadFloat3(&meshData.Vertices[i].Position));
            const DirectX::XMVECTOR position = DirectX::XMVectorScale(normal, config.Radius);

            DirectX::XMStoreFloat3(&meshData.Vertices[i].Position, position);
            DirectX::XMStoreFloat3(&meshData.Vertices[i].Normal, normal);

            float theta = std::atan2(meshData.Vertices[i].Position.z, meshData.Vertices[i].Position.x);

            if (theta < 0.0f)
            {
                theta += DirectX::XM_2PI;
            }

            const float phi = std::acosf(meshData.Vertices[i].Position.y / config.Radius);

            meshData.Vertices[i].TexCoord.x = theta / DirectX::XM_2PI;
            meshData.Vertices[i].TexCoord.y = phi / DirectX::XM_PI;

            meshData.Vertices[i].Tangent.x = -config.Radius * std::sin(phi) * std::sin(theta);
            meshData.Vertices[i].Tangent.y = 0.0f;
            meshData.Vertices[i].Tangent.z = config.Radius * std::sin(phi) * std::cos(theta);

            DirectX::XMStoreFloat3(&meshData.Vertices[i].Tangent, DirectX::XMVector3Normalize(DirectX::XMLoadFloat3(&meshData.Vertices[i].Tangent)));
        }

        return meshData;
    }

} // namespace benzin::geometry_generator
