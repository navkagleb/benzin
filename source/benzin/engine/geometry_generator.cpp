#include "benzin/config/bootstrap.hpp"
#include "benzin/engine/geometry_generator.hpp"

#include "benzin/engine/mesh.hpp"

namespace benzin
{

    namespace
    {

        MeshVertex MiddlePoint(const MeshVertex& lhs, const MeshVertex& rhs)
        {
            const DirectX::XMVECTOR positionLhs = DirectX::XMLoadFloat3(&lhs.Position);
            const DirectX::XMVECTOR positionRhs = DirectX::XMLoadFloat3(&rhs.Position);

            const DirectX::XMVECTOR normalLhs = DirectX::XMLoadFloat3(&lhs.Normal);
            const DirectX::XMVECTOR normalRhs = DirectX::XMLoadFloat3(&rhs.Normal);

            const DirectX::XMVECTOR texCoordLhs = DirectX::XMLoadFloat2(&lhs.TexCoord);
            const DirectX::XMVECTOR texCoordRhs = DirectX::XMLoadFloat2(&rhs.TexCoord);

            // Compute the midpoints of all the attributes. Vectors need to be normalized
            // since linear interpolating can make them not unit length. 
            const DirectX::XMVECTOR localPosition = DirectX::XMVectorScale(DirectX::XMVectorAdd(positionLhs, positionRhs), 0.5f);
            const DirectX::XMVECTOR localNormal = DirectX::XMVector3Normalize(DirectX::XMVectorScale(DirectX::XMVectorAdd(normalLhs, normalRhs), 0.5f));
            const DirectX::XMVECTOR texCoord = DirectX::XMVectorScale(DirectX::XMVectorAdd(texCoordLhs, texCoordRhs), 0.5f);

            MeshVertex middle;
            DirectX::XMStoreFloat3(&middle.Position, localPosition);
            DirectX::XMStoreFloat3(&middle.Normal, localNormal);
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
                const MeshVertex vertex0 = copy.Vertices[copy.Indices[i * 3 + 0]];
                const MeshVertex vertex1 = copy.Vertices[copy.Indices[i * 3 + 1]];
                const MeshVertex vertex2 = copy.Vertices[copy.Indices[i * 3 + 2]];

                const MeshVertex middle0 = MiddlePoint(vertex0, vertex1);
                const MeshVertex middle1 = MiddlePoint(vertex1, vertex2);
                const MeshVertex middle2 = MiddlePoint(vertex0, vertex2);

                meshData.Vertices.push_back(vertex0); // 0
                meshData.Vertices.push_back(vertex1); // 1
                meshData.Vertices.push_back(vertex2); // 2
                meshData.Vertices.push_back(middle0); // 3
                meshData.Vertices.push_back(middle1); // 4
                meshData.Vertices.push_back(middle2); // 5

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

        void GenerateCylinderTopCap(const CylinderGeometryCreation& creation, MeshData& meshData)
        {
            const uint32_t baseIndex = static_cast<uint32_t>(meshData.Vertices.size());

            // Vertices
            {
                const float y = 0.5f * creation.Height;
                const float dTheta = DirectX::XM_2PI / creation.SliceCount;

                for (uint32_t i = 0; i <= creation.SliceCount; ++i)
                {
                    const float x = creation.TopRadius * std::cos(i * dTheta);
                    const float z = creation.TopRadius * std::sin(i * dTheta);
                    const float u = x / creation.Height + 0.5f;
                    const float v = z / creation.Height + 0.5f;

                    meshData.Vertices.push_back(MeshVertex
                    {
                        .Position{ x, y, z },
                        .Normal{ 0.0f, 1.0f, 0.0f },
                        .TexCoord{ u, v }
                    });
                }

                const MeshVertex centerVertex
                {
                    .Position{ 0.0f, y, 0.0f },
                    .Normal{ 0.0f, 1.0f, 0.0f },
                    .TexCoord{ 0.5f, 0.5f }
                };

                meshData.Vertices.push_back(centerVertex);
            }

            // Indices
            {
                const uint32_t centerIndex = static_cast<uint32_t>(meshData.Vertices.size() - 1);

                for (uint32_t i = 0; i < creation.SliceCount; ++i)
                {
                    meshData.Indices.push_back(centerIndex);
                    meshData.Indices.push_back(baseIndex + i + 1);
                    meshData.Indices.push_back(baseIndex + i);
                }
            }
        }

        void GenerateCylinderBottomCap(const CylinderGeometryCreation& creation, MeshData& meshData)
        {
            const uint32_t baseIndex = static_cast<uint32_t>(meshData.Vertices.size());

            // Vertices
            {
                const float y = -0.5f * creation.Height;
                const float dTheta = DirectX::XM_2PI / creation.SliceCount;

                for (uint32_t i = 0; i <= creation.SliceCount; ++i)
                {
                    const float x = creation.BottomRadius * cosf(i * dTheta);
                    const float z = creation.BottomRadius * sinf(i * dTheta);
                    const float u = x / creation.Height + 0.5f;
                    const float v = z / creation.Height + 0.5f;

                    meshData.Vertices.push_back(MeshVertex
                    {
                        .Position{ x, y, z },
                        .Normal{ 0.0f, -1.0f, 0.0f },
                        .TexCoord{ u, v }
                    });
                }

                const MeshVertex centerVertex
                {
                    .Position{ 0.0f, y, 0.0f },
                    .Normal{ 0.0f, -1.0f, 0.0f },
                    .TexCoord{ 0.5f, 0.5f },
                };

                meshData.Vertices.push_back(centerVertex);
            }

            // Indices
            {
                const uint32_t centerIndex = static_cast<uint32_t>(meshData.Vertices.size() - 1);

                for (uint32_t i = 0; i < creation.SliceCount; ++i)
                {
                    meshData.Indices.push_back(centerIndex);
                    meshData.Indices.push_back(baseIndex + i);
                    meshData.Indices.push_back(baseIndex + i + 1);
                }
            }
        }

    } // anonymous namespace

    MeshData GenerateBox(const BoxGeometryCreation& creation)
    {
        MeshData meshData;
        meshData.PrimitiveTopology = PrimitiveTopology::TriangleList;

        const float w2 = 0.5f * creation.Width;
        const float h2 = 0.5f * creation.Height;
        const float d2 = 0.5f * creation.Depth;

        auto& vertices = meshData.Vertices;
        vertices.resize(24);

        // Front face
        vertices[0] = MeshVertex{ DirectX::XMFLOAT3{ -w2, -h2, -d2 }, DirectX::XMFLOAT3{ 0.0f, 0.0f, -1.0f }, DirectX::XMFLOAT2{ 0.0f, 1.0f } };
        vertices[1] = MeshVertex{ DirectX::XMFLOAT3{ -w2,  h2, -d2 }, DirectX::XMFLOAT3{ 0.0f, 0.0f, -1.0f }, DirectX::XMFLOAT2{ 0.0f, 0.0f } };
        vertices[2] = MeshVertex{ DirectX::XMFLOAT3{  w2,  h2, -d2 }, DirectX::XMFLOAT3{ 0.0f, 0.0f, -1.0f }, DirectX::XMFLOAT2{ 1.0f, 0.0f } };
        vertices[3] = MeshVertex{ DirectX::XMFLOAT3{  w2, -h2, -d2 }, DirectX::XMFLOAT3{ 0.0f, 0.0f, -1.0f }, DirectX::XMFLOAT2{ 1.0f, 1.0f } };

        // Back face
        vertices[4] = MeshVertex{ DirectX::XMFLOAT3{ -w2, -h2,  d2 }, DirectX::XMFLOAT3{ 0.0f, 0.0f, 1.0f }, DirectX::XMFLOAT2{ 1.0f, 1.0f } };
        vertices[5] = MeshVertex{ DirectX::XMFLOAT3{  w2, -h2,  d2 }, DirectX::XMFLOAT3{ 0.0f, 0.0f, 1.0f }, DirectX::XMFLOAT2{ 0.0f, 1.0f } };
        vertices[6] = MeshVertex{ DirectX::XMFLOAT3{  w2,  h2,  d2 }, DirectX::XMFLOAT3{ 0.0f, 0.0f, 1.0f }, DirectX::XMFLOAT2{ 0.0f, 0.0f } };
        vertices[7] = MeshVertex{ DirectX::XMFLOAT3{ -w2,  h2,  d2 }, DirectX::XMFLOAT3{ 0.0f, 0.0f, 1.0f }, DirectX::XMFLOAT2{ 1.0f, 0.0f } };

        // Top face
        vertices[8] = MeshVertex{ DirectX::XMFLOAT3{ -w2,  h2, -d2}, DirectX::XMFLOAT3{ 0.0f, 1.0f, 0.0f }, DirectX::XMFLOAT2{ 0.0f, 1.0f } };
        vertices[9] = MeshVertex{ DirectX::XMFLOAT3{ -w2,  h2,  d2}, DirectX::XMFLOAT3{ 0.0f, 1.0f, 0.0f }, DirectX::XMFLOAT2{ 0.0f, 0.0f } };
        vertices[10] = MeshVertex{ DirectX::XMFLOAT3{  w2,  h2,  d2}, DirectX::XMFLOAT3{ 0.0f, 1.0f, 0.0f }, DirectX::XMFLOAT2{ 1.0f, 0.0f } };
        vertices[11] = MeshVertex{ DirectX::XMFLOAT3{  w2,  h2, -d2}, DirectX::XMFLOAT3{ 0.0f, 1.0f, 0.0f }, DirectX::XMFLOAT2{ 1.0f, 1.0f } };

        // Bottom face
        vertices[12] = MeshVertex{ DirectX::XMFLOAT3{ -w2, -h2, -d2 }, DirectX::XMFLOAT3{ 0.0f, -1.0f, 0.0f}, DirectX::XMFLOAT2{ 1.0f, 1.0f} };
        vertices[13] = MeshVertex{ DirectX::XMFLOAT3{  w2, -h2, -d2 }, DirectX::XMFLOAT3{ 0.0f, -1.0f, 0.0f}, DirectX::XMFLOAT2{ 0.0f, 1.0f} };
        vertices[14] = MeshVertex{ DirectX::XMFLOAT3{  w2, -h2,  d2 }, DirectX::XMFLOAT3{ 0.0f, -1.0f, 0.0f}, DirectX::XMFLOAT2{ 0.0f, 0.0f} };
        vertices[15] = MeshVertex{ DirectX::XMFLOAT3{ -w2, -h2,  d2 }, DirectX::XMFLOAT3{ 0.0f, -1.0f, 0.0f}, DirectX::XMFLOAT2{ 1.0f, 0.0f} };

        // Left face
        vertices[16] = MeshVertex{ DirectX::XMFLOAT3{ -w2, -h2,  d2 }, DirectX::XMFLOAT3{ -1.0f, 0.0f, 0.0f }, DirectX::XMFLOAT2{ 0.0f, 1.0f } };
        vertices[17] = MeshVertex{ DirectX::XMFLOAT3{ -w2,  h2,  d2 }, DirectX::XMFLOAT3{ -1.0f, 0.0f, 0.0f }, DirectX::XMFLOAT2{ 0.0f, 0.0f } };
        vertices[18] = MeshVertex{ DirectX::XMFLOAT3{ -w2,  h2, -d2 }, DirectX::XMFLOAT3{ -1.0f, 0.0f, 0.0f }, DirectX::XMFLOAT2{ 1.0f, 0.0f } };
        vertices[19] = MeshVertex{ DirectX::XMFLOAT3{ -w2, -h2, -d2 }, DirectX::XMFLOAT3{ -1.0f, 0.0f, 0.0f }, DirectX::XMFLOAT2{ 1.0f, 1.0f } };

        // Right face
        vertices[20] = MeshVertex{ DirectX::XMFLOAT3{  w2, -h2, -d2 }, DirectX::XMFLOAT3{ 1.0f, 0.0f, 0.0f }, DirectX::XMFLOAT2{ 0.0f, 1.0f } };
        vertices[21] = MeshVertex{ DirectX::XMFLOAT3{  w2,  h2, -d2 }, DirectX::XMFLOAT3{ 1.0f, 0.0f, 0.0f }, DirectX::XMFLOAT2{ 0.0f, 0.0f } };
        vertices[22] = MeshVertex{ DirectX::XMFLOAT3{  w2,  h2,  d2 }, DirectX::XMFLOAT3{ 1.0f, 0.0f, 0.0f }, DirectX::XMFLOAT2{ 1.0f, 0.0f } };
        vertices[23] = MeshVertex{ DirectX::XMFLOAT3{  w2, -h2,  d2 }, DirectX::XMFLOAT3{ 1.0f, 0.0f, 0.0f }, DirectX::XMFLOAT2{ 1.0f, 1.0f } };

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

        const uint32_t subdivisionCount = std::min<uint32_t>(creation.SubdivisionCount, 6);

        for (uint32_t i = 0; i < subdivisionCount; ++i)
        {
            Subdivide(meshData);
        }

        return meshData;
    }

    MeshData GenerateGrid(const GridGeometryCreation& creation)
    {
        MeshData meshData;
        meshData.PrimitiveTopology = PrimitiveTopology::TriangleList;

        const uint32_t vertexCount = creation.RowCount * creation.ColumnCount;
        const uint32_t faceCount = (creation.RowCount - 1) * (creation.ColumnCount - 1) * 2;

        // Vertices
        {
            const float halfWidth = 0.5f * creation.Width;
            const float halfDepth = 0.5f * creation.Depth;

            const float dx = creation.Width / (creation.RowCount - 1);
            const float dz = creation.Depth / (creation.ColumnCount - 1);

            const float du = 1.0f / (creation.RowCount - 1);
            const float dv = 1.0f / (creation.ColumnCount - 1);

            auto& vertices = meshData.Vertices;
            vertices.resize(vertexCount);

            for (uint32_t i = 0; i < creation.RowCount; ++i)
            {
                const float z = halfDepth - i * dz;

                for (uint32_t j = 0; j < creation.ColumnCount; ++j)
                {
                    const float x = -halfWidth + j * dx;

                    vertices[i * creation.ColumnCount + j].Position = DirectX::XMFLOAT3{ x, 0.0f, z };
                    vertices[i * creation.ColumnCount + j].Normal = DirectX::XMFLOAT3{ 0.0f, 1.0f, 0.0f };
                    vertices[i * creation.ColumnCount + j].TexCoord = DirectX::XMFLOAT2{ j * du, i * dv };
                }
            }
        }

        // Indices
        {
            auto& indices = meshData.Indices;
            indices.resize(faceCount * 3);

            size_t baseIndex = 0;

            for (uint32_t i = 0; i < creation.RowCount - 1; ++i)
            {
                for (uint32_t j = 0; j < creation.ColumnCount - 1; ++j)
                {
                    indices[baseIndex + 0] = i * creation.ColumnCount + j;
                    indices[baseIndex + 1] = i * creation.ColumnCount + j + 1;
                    indices[baseIndex + 2] = (i + 1) * creation.ColumnCount + j;

                    indices[baseIndex + 3] = (i + 1) * creation.ColumnCount + j;
                    indices[baseIndex + 4] = i * creation.ColumnCount + j + 1;
                    indices[baseIndex + 5] = (i + 1) * creation.ColumnCount + j + 1;

                    baseIndex += 6;
                }
            }
        }

        return meshData;
    }

    MeshData GenerateCylinder(const CylinderGeometryCreation& creation)
    {
        MeshData meshData;
        meshData.PrimitiveTopology = PrimitiveTopology::TriangleList;

        // Vertices
        {
            const float stackHeight = creation.Height / static_cast<float>(creation.StackCount);
            const float radiusStep = (creation.TopRadius - creation.BottomRadius) / static_cast<float>(creation.StackCount);

            const uint32_t ringCount = creation.StackCount + 1;

            for (uint32_t i = 0; i < ringCount; ++i)
            {
                const float y = -0.5f * creation.Height + static_cast<float>(i) * stackHeight;
                const float r = creation.BottomRadius + static_cast<float>(i) * radiusStep;

                const float dTheta = DirectX::XM_2PI / static_cast<float>(creation.SliceCount);

                // Ring Vertices
                for (uint32_t j = 0; j <= creation.SliceCount; ++j)
                {
                    MeshVertex& vertex = meshData.Vertices.emplace_back();

                    const float c = std::cos(j * dTheta);
                    const float s = std::sin(j * dTheta);

                    vertex.Position = DirectX::XMFLOAT3{ r * c, y, r * s };

                    vertex.TexCoord.x = static_cast<float>(j) / creation.SliceCount;
                    vertex.TexCoord.y = 1.0f - static_cast<float>(i) / creation.StackCount;

                    const DirectX::XMFLOAT3 tangent{ -s, 0.0f, c };

                    const float dr = creation.BottomRadius - creation.TopRadius;
                    const DirectX::XMFLOAT3 bitangent{ dr * c, -creation.Height, dr * s };

                    const DirectX::XMVECTOR t = DirectX::XMLoadFloat3(&tangent);
                    const DirectX::XMVECTOR b = DirectX::XMLoadFloat3(&bitangent);
                    const DirectX::XMVECTOR n = DirectX::XMVector3Normalize(DirectX::XMVector3Cross(t, b));

                    DirectX::XMStoreFloat3(&vertex.Normal, n);
                }
            }
        }

        // Indices
        {
            const uint32_t ringVertexCount = creation.SliceCount + 1;

            for (uint32_t i = 0; i < creation.StackCount; ++i)
            {
                for (uint32_t j = 0; j < creation.SliceCount; ++j)
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

        GenerateCylinderTopCap(creation, meshData);
        GenerateCylinderBottomCap(creation, meshData);

        return meshData;
    }

    MeshData GenerateSphere(const SphereGeometryCreation& creation)
    {
        MeshData meshData;
        meshData.PrimitiveTopology = PrimitiveTopology::TriangleList;

        // Vertices
        {
            const MeshVertex topVertex
            {
                .Position{ 0.0f, creation.Radius, 0.0f },
                .Normal{ 0.0f, 1.0f, 0.0f },
                .TexCoord{ 0.0f, 0.0f }
            };

            const MeshVertex bottomVertex
            {
                .Position{ 0.0f, -creation.Radius, 0.0f },
                .Normal{ 0.0f, -1.0f, 0.0f },
                .TexCoord{ 0.0f, 1.0f },
            };

            meshData.Vertices.push_back(topVertex);

            const float phiStep = DirectX::XM_PI / static_cast<float>(creation.StackCount);
            const float thetaStep = DirectX::XM_2PI / static_cast<float>(creation.SliceCount);

            for (uint32_t i = 1; i < creation.StackCount; ++i)
            {
                const float phi = i * phiStep;

                for (uint32_t j = 0; j <= creation.SliceCount; ++j)
                {
                    const float theta = j * thetaStep;

                    MeshVertex& vertex = meshData.Vertices.emplace_back();

                    // Spherical to cartesian
                    vertex.Position.x = creation.Radius * std::sin(phi) * std::cos(theta);
                    vertex.Position.y = creation.Radius * std::cos(phi);
                    vertex.Position.z = creation.Radius * std::sin(phi) * std::sin(theta);

                    DirectX::XMStoreFloat3(&vertex.Normal, DirectX::XMVector3Normalize(DirectX::XMLoadFloat3(&vertex.Position)));

                    vertex.TexCoord.x = theta / DirectX::XM_2PI;
                    vertex.TexCoord.y = phi / DirectX::XM_PI;
                }
            }

            meshData.Vertices.push_back(bottomVertex);
        }

        // Indices
        {
            for (uint32_t i = 1; i <= creation.SliceCount; ++i)
            {
                meshData.Indices.push_back(0);
                meshData.Indices.push_back(i + 1);
                meshData.Indices.push_back(i);
            }

            const uint32_t ringVertexCount = creation.SliceCount + 1;

            uint32_t baseIndex = 1;

            for (uint32_t i = 0; i < creation.StackCount - 2; ++i)
            {
                for (uint32_t j = 0; j < creation.SliceCount; ++j)
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

            for (uint32_t i = 0; i < creation.SliceCount; ++i)
            {
                meshData.Indices.push_back(southPoleIndex);
                meshData.Indices.push_back(baseIndex + i);
                meshData.Indices.push_back(baseIndex + i + 1);
            }
        }

        return meshData;
    }

    MeshData GenerateGeosphere(const GeosphereGeometryCreation& creation)
    {
        static constexpr float x = 0.525731f;
        static constexpr float z = 0.850651f;

        static constexpr auto positions = std::to_array(
        {
            DirectX::XMFLOAT3{ -x,     0.0f,  z    }, DirectX::XMFLOAT3{  x,     0.0f,  z    },
            DirectX::XMFLOAT3{ -x,     0.0f, -z    }, DirectX::XMFLOAT3{  x,     0.0f, -z    },
            DirectX::XMFLOAT3{  0.0f,  z,     x    }, DirectX::XMFLOAT3{  0.0f,  z,    -x    },
            DirectX::XMFLOAT3{  0.0f, -z,     x    }, DirectX::XMFLOAT3{  0.0f, -z,    -x    },
            DirectX::XMFLOAT3{  z,     x,     0.0f }, DirectX::XMFLOAT3{ -z,     x,     0.0f },
            DirectX::XMFLOAT3{  z,    -x,     0.0f }, DirectX::XMFLOAT3{ -z,    -x,     0.0f }
        });

        static constexpr auto indicies = std::to_array(
        {
            1,  4,  0,      4,  9, 0,       4, 5,  9,       8, 5, 4,        1,  8, 4,
            1,  10, 8,      10, 3, 8,       8, 3,  5,       3, 2, 5,        3,  7, 2,
            3,  10, 7,      10, 6, 7,       6, 11, 7,       6, 0, 11,       6,  1, 0,
            10, 1,  6,      11, 0, 9,       2, 11, 9,       5, 2, 9,        11, 2, 7
        });

        MeshData meshData;
        meshData.PrimitiveTopology = PrimitiveTopology::TriangleList;
        meshData.Vertices.insert(meshData.Vertices.begin(), positions.begin(), positions.end());
        meshData.Indices.insert(meshData.Indices.begin(), indicies.begin(), indicies.end());
        
        const uint32_t subdivisionCount = std::min<uint32_t>(creation.SubdivisionCount, 6);
        for (uint32_t i = 0; i < subdivisionCount; ++i)
        {
            Subdivide(meshData);
        }

        for (uint32_t i = 0; i < meshData.Vertices.size(); ++i)
        {
            const DirectX::XMVECTOR normal = DirectX::XMVector3Normalize(DirectX::XMLoadFloat3(&meshData.Vertices[i].Position));
            const DirectX::XMVECTOR position = DirectX::XMVectorScale(normal, creation.Radius);

            DirectX::XMStoreFloat3(&meshData.Vertices[i].Position, position);
            DirectX::XMStoreFloat3(&meshData.Vertices[i].Normal, normal);

            float theta = std::atan2(meshData.Vertices[i].Position.z, meshData.Vertices[i].Position.x);

            if (theta < 0.0f)
            {
                theta += DirectX::XM_2PI;
            }

            const float phi = std::acosf(meshData.Vertices[i].Position.y / creation.Radius);

            meshData.Vertices[i].TexCoord.x = theta / DirectX::XM_2PI;
            meshData.Vertices[i].TexCoord.y = phi / DirectX::XM_PI;
        }

        return meshData;
    }

} // namespace benzin
