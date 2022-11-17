#include "spieler/config/bootstrap.hpp"

#include "spieler/engine/geometry_generator.hpp"

namespace spieler
{

    MeshData GeometryGenerator::GenerateBox(const BoxGeometryConfig& props)
    {
        MeshData meshData;

        const float w2{ 0.5f * props.Width };
        const float h2{ 0.5f * props.Height };
        const float d2{ 0.5f * props.Depth };

        auto& vertices{ meshData.Vertices };
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
        vertices[8]  = Vertex{ DirectX::XMFLOAT3{ -w2,  h2, -d2}, DirectX::XMFLOAT3{ 0.0f, 1.0f, 0.0f }, DirectX::XMFLOAT3{ 1.0f, 0.0f, 0.0f }, DirectX::XMFLOAT2{ 0.0f, 1.0f } };
        vertices[9]  = Vertex{ DirectX::XMFLOAT3{ -w2,  h2,  d2}, DirectX::XMFLOAT3{ 0.0f, 1.0f, 0.0f }, DirectX::XMFLOAT3{ 1.0f, 0.0f, 0.0f }, DirectX::XMFLOAT2{ 0.0f, 0.0f } };
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

        const uint32_t subdivisionCount{ std::min<uint32_t>(props.SubdivisionCount, 6) };

        for (uint32_t i = 0; i < subdivisionCount; ++i)
        {
            Subdivide(meshData);
        }

        return meshData;
    }

    MeshData GeometryGenerator::GenerateGrid(const GridGeometryConfig& props)
    {
        MeshData meshData;

        const uint32_t vertexCount{ props.RowCount * props.ColumnCount };
        const uint32_t faceCount{ (props.RowCount - 1) * (props.ColumnCount - 1) * 2 };

        // Vertices
        {
            const float halfWidth{ 0.5f * props.Width };
            const float halfDepth{ 0.5f * props.Depth };

            const float dx{ props.Width / (props.RowCount - 1) };
            const float dz{ props.Depth / (props.ColumnCount - 1) };

            const float du{ 1.0f / (props.RowCount - 1) };
            const float dv{ 1.0f / (props.ColumnCount - 1) };

            auto& vertices{ meshData.Vertices };
            vertices.resize(vertexCount);

            for (uint32_t i = 0; i < props.RowCount; ++i)
            {
                const float z{ halfDepth - i * dz };

                for (uint32_t j = 0; j < props.ColumnCount; ++j)
                {
                    const float x{ -halfWidth + j * dx };

                    vertices[i * props.ColumnCount + j].Position = DirectX::XMFLOAT3{ x, 0.0f, z };
                    vertices[i * props.ColumnCount + j].Normal = DirectX::XMFLOAT3{ 0.0f, 1.0f, 0.0f };
                    vertices[i * props.ColumnCount + j].Tangent = DirectX::XMFLOAT3{ 1.0f, 0.0f, 0.0f };
                    vertices[i * props.ColumnCount + j].TexCoord = DirectX::XMFLOAT2{ j * du, i * dv };
                }
            }
        }
        
        // Indices
        {
            auto& indices{ meshData.Indices };
            indices.resize(faceCount * 3);

            uint32_t baseIndex{ 0 };

            for (uint32_t i = 0; i < props.RowCount - 1; ++i)
            {
                for (uint32_t j = 0; j < props.ColumnCount - 1; ++j)
                {
                    indices[baseIndex + 0] = i * props.ColumnCount + j;
                    indices[baseIndex + 1] = i * props.ColumnCount + j + 1;
                    indices[baseIndex + 2] = (i + 1) * props.ColumnCount + j;

                    indices[baseIndex + 3] = (i + 1) * props.ColumnCount + j;
                    indices[baseIndex + 4] = i * props.ColumnCount + j + 1;
                    indices[baseIndex + 5] = (i + 1) * props.ColumnCount + j + 1;

                    baseIndex += 6;
                }
            }
        }
        
        return meshData;
    }

    MeshData GeometryGenerator::GenerateCylinder(const CylinderGeometryConfig& props)
    {
        MeshData meshData;

        // Vertices
        {
            const float stackHeight{ props.Height / static_cast<float>(props.StackCount) };
            const float radiusStep{ (props.TopRadius - props.BottomRadius) / static_cast<float>(props.StackCount) };

            const uint32_t ringCount{ props.StackCount + 1 };

            for (uint32_t i = 0; i < ringCount; ++i)
            {
                const float y{ -0.5f * props.Height + static_cast<float>(i) * stackHeight };
                const float r{ props.BottomRadius + static_cast<float>(i) * radiusStep };

                const float dTheta = 2.0f * static_cast<float>(std::numbers::pi) / static_cast<float>(props.SliceCount);

                // Ring Vertices
                for (uint32_t j = 0; j <= props.SliceCount; ++j)
                {
                    Vertex vertex;

                    const float c{ std::cos(j * dTheta) };
                    const float s{ std::sin(j * dTheta) };

                    vertex.Position = DirectX::XMFLOAT3{ r * c, y, r * s };

                    vertex.TexCoord.x = static_cast<float>(j) / props.SliceCount;
                    vertex.TexCoord.y = 1.0f - static_cast<float>(i) / props.StackCount;

                    vertex.Tangent = DirectX::XMFLOAT3{ -s, 0.0f, c };

                    const float dr{ props.BottomRadius - props.TopRadius };

                    const DirectX::XMFLOAT3 bitangent{ dr * c, -props.Height, dr * s };

                    const DirectX::XMVECTOR T{ DirectX::XMLoadFloat3(&vertex.Tangent) };
                    const DirectX::XMVECTOR B{ DirectX::XMLoadFloat3(&bitangent) };
                    const DirectX::XMVECTOR N{ DirectX::XMVector3Normalize(DirectX::XMVector3Cross(T, B)) };

                    DirectX::XMStoreFloat3(&vertex.Normal, N);

                    meshData.Vertices.push_back(vertex);
                }
            }
        }

        // Indices
        {
            const uint32_t ringVertexCount{ props.SliceCount + 1 };

            for (uint32_t i = 0; i < props.StackCount; ++i)
            {
                for (uint32_t j = 0; j < props.SliceCount; ++j)
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

        GenerateCylinderTopCap(props, meshData);
        GenerateCylinderBottomCap(props, meshData);

        return meshData;
    }

    MeshData GeometryGenerator::GenerateSphere(const SphereGeometryConfig& props)
    {
        MeshData meshData;

        // Vertices
        {
            const Vertex topVertex
            {
                .Position{ 0.0f, props.Radius, 0.0f },
                .Normal{ 0.0f, 1.0f, 0.0f },
                .Tangent{ 1.0f, 0.0f, 0.0f },
                .TexCoord{ 0.0f, 0.0f }
            };

            const Vertex bottomVertex
            {
                .Position{ 0.0f, -props.Radius, 0.0f },
                .Normal{ 0.0f, -1.0f, 0.0f },
                .Tangent{ 1.0f, 0.0f, 0.0f },
                .TexCoord{ 0.0f, 1.0f },
            };

            meshData.Vertices.push_back(topVertex);

            const float phiStep{ DirectX::XM_PI / props.StackCount };
            const float thetaStep{ DirectX::XM_2PI / props.SliceCount };

            for (uint32_t i = 1; i < props.StackCount; ++i)
            {
                const float phi{ i * phiStep };

                for (uint32_t j = 0; j <= props.SliceCount; ++j)
                {
                    const float theta{ j * thetaStep };

                    Vertex vertex;

                    // Spherical to cartesian
                    vertex.Position.x = props.Radius * std::sin(phi) * std::cos(theta);
                    vertex.Position.y = props.Radius * std::cos(phi);
                    vertex.Position.z = props.Radius * std::sin(phi) * std::sin(theta);

                    // Partial derivative of P with respect to theta
                    vertex.Tangent.x = -props.Radius * std::sin(phi) * std::sin(theta);
                    vertex.Tangent.y = 0.0f;
                    vertex.Tangent.z = props.Radius * std::sin(phi) * std::cos(theta);

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
            for (uint32_t i = 1; i <= props.SliceCount; ++i)
            {
                meshData.Indices.push_back(0);
                meshData.Indices.push_back(i + 1);
                meshData.Indices.push_back(i);
            }

            const uint32_t ringVertexCount{ props.SliceCount + 1 };

            uint32_t baseIndex{ 1 };

            for (uint32_t i = 0; i < props.StackCount - 2; ++i)
            {
                for (uint32_t j = 0; j < props.SliceCount; ++j)
                {
                    meshData.Indices.push_back(baseIndex + i * ringVertexCount + j);
                    meshData.Indices.push_back(baseIndex + i * ringVertexCount + j + 1);
                    meshData.Indices.push_back(baseIndex + (i + 1) * ringVertexCount + j);

                    meshData.Indices.push_back(baseIndex + (i + 1) * ringVertexCount + j);
                    meshData.Indices.push_back(baseIndex + i * ringVertexCount + j + 1);
                    meshData.Indices.push_back(baseIndex + (i + 1) * ringVertexCount + j + 1);
                }
            }

            const auto southPoleIndex{ static_cast<uint32_t>(meshData.Vertices.size() - 1) };

            baseIndex = southPoleIndex - ringVertexCount;

            for (uint32_t i = 0; i < props.SliceCount; ++i)
            {
                meshData.Indices.push_back(southPoleIndex);
                meshData.Indices.push_back(baseIndex + i);
                meshData.Indices.push_back(baseIndex + i + 1);
            }
        }

        return meshData;
    }

    MeshData GeometryGenerator::GenerateGeosphere(const GeosphereGeometryConfig& props)
    {
        MeshData meshData;

        // Put a cap on the number of subdivisions
        const uint32_t subdivisionCount{ std::min<uint32_t>(props.SubdivisionCount, 6) };

        // Approximate a sphere by tessellating an icosahedron
        const float x{ 0.525731f };
        const float z{ 0.850651f };

        const std::array<DirectX::XMFLOAT3, 12> positions
        {
            DirectX::XMFLOAT3{ -x,     0.0f,  z    }, DirectX::XMFLOAT3{  x,     0.0f,  z    },
            DirectX::XMFLOAT3{ -x,     0.0f, -z    }, DirectX::XMFLOAT3{  x,     0.0f, -z    },
            DirectX::XMFLOAT3{  0.0f,  z,     x    }, DirectX::XMFLOAT3{  0.0f,  z,    -x    },
            DirectX::XMFLOAT3{  0.0f, -z,     x    }, DirectX::XMFLOAT3{  0.0f, -z,    -x    },
            DirectX::XMFLOAT3{  z,     x,     0.0f }, DirectX::XMFLOAT3{ -z,     x,     0.0f },
            DirectX::XMFLOAT3{  z,    -x,     0.0f }, DirectX::XMFLOAT3{ -z,    -x,     0.0f }
        };

        meshData.Vertices.resize(positions.size());

        for (uint32_t i = 0; i < positions.size(); ++i)
        {
            meshData.Vertices[i].Position = positions[i];
        }

        meshData.Indices =
        {
            1,  4,  0,      4,  9, 0,       4, 5,  9,       8, 5, 4,        1,  8, 4,
            1,  10, 8,      10, 3, 8,       8, 3,  5,       3, 2, 5,        3,  7, 2,
            3,  10, 7,      10, 6, 7,       6, 11, 7,       6, 0, 11,       6,  1, 0,
            10, 1,  6,      11, 0, 9,       2, 11, 9,       5, 2, 9,        11, 2, 7
        };

        for (uint32_t i = 0; i < subdivisionCount; ++i)
        {
            Subdivide(meshData);
        }

        // Project vertices onto sphere and scale
        for (uint32_t i = 0; i < meshData.Vertices.size(); ++i)
        {
            // Project onto unit sphere
            const DirectX::XMVECTOR normal{ DirectX::XMVector3Normalize(DirectX::XMLoadFloat3(&meshData.Vertices[i].Position)) };

            // Project onto sphere
            const DirectX::XMVECTOR position{ DirectX::XMVectorScale(normal, props.Radius) };

            DirectX::XMStoreFloat3(&meshData.Vertices[i].Position, position);
            DirectX::XMStoreFloat3(&meshData.Vertices[i].Normal, normal);

            // Derive texture coordinates from spherical coordinates
            float theta = std::atan2(meshData.Vertices[i].Position.z, meshData.Vertices[i].Position.x);

            // Put in [0, 2pi]
            if (theta < 0.0f)
            {
                theta += DirectX::XM_2PI;
            }

            const float phi = std::acosf(meshData.Vertices[i].Position.y / props.Radius);

            meshData.Vertices[i].TexCoord.x = theta / DirectX::XM_2PI;
            meshData.Vertices[i].TexCoord.y = phi / DirectX::XM_PI;

            // Partial derivative of P with respect to theta
            meshData.Vertices[i].Tangent.x = -props.Radius * std::sin(phi) * std::sin(theta);
            meshData.Vertices[i].Tangent.y = 0.0f;
            meshData.Vertices[i].Tangent.z = props.Radius * std::sin(phi) * std::cos(theta);

            DirectX::XMStoreFloat3(&meshData.Vertices[i].Tangent, DirectX::XMVector3Normalize(DirectX::XMLoadFloat3(&meshData.Vertices[i].Tangent)));
        }

        return meshData;
    }

    Vertex GeometryGenerator::MiddlePoint(const Vertex& lhs, const Vertex& rhs)
    {
        const DirectX::XMVECTOR positionLhs{ DirectX::XMLoadFloat3(&lhs.Position) };
        const DirectX::XMVECTOR positionRhs{ DirectX::XMLoadFloat3(&rhs.Position) };

        const DirectX::XMVECTOR normalLhs{ DirectX::XMLoadFloat3(&lhs.Normal) };
        const DirectX::XMVECTOR normalRhs{ DirectX::XMLoadFloat3(&rhs.Normal) };

        const DirectX::XMVECTOR tangentLhs{ DirectX::XMLoadFloat3(&lhs.Tangent) };
        const DirectX::XMVECTOR tangentRhs{ DirectX::XMLoadFloat3(&rhs.Tangent) };

        const DirectX::XMVECTOR texCoordLhs{ DirectX::XMLoadFloat2(&lhs.TexCoord) };
        const DirectX::XMVECTOR texCoordRhs{ DirectX::XMLoadFloat2(&rhs.TexCoord) };

        // Compute the midpoints of all the attributes.  Vectors need to be normalized
        // since linear interpolating can make them not unit length.  
        const DirectX::XMVECTOR position{ DirectX::XMVectorScale(DirectX::XMVectorAdd(positionLhs, positionRhs), 0.5f) };
        const DirectX::XMVECTOR normal{ DirectX::XMVector3Normalize(DirectX::XMVectorScale(DirectX::XMVectorAdd(normalLhs, normalRhs), 0.5f)) };
        const DirectX::XMVECTOR tangent{ DirectX::XMVector3Normalize(DirectX::XMVectorScale(DirectX::XMVectorAdd(tangentLhs, tangentRhs), 0.5f)) };
        const DirectX::XMVECTOR texCoord{ DirectX::XMVectorScale(DirectX::XMVectorAdd(texCoordLhs, texCoordRhs), 0.5f) };

        Vertex middle;
        DirectX::XMStoreFloat3(&middle.Position, position);
        DirectX::XMStoreFloat3(&middle.Normal, normal);
        DirectX::XMStoreFloat3(&middle.Tangent, tangent);
        DirectX::XMStoreFloat2(&middle.TexCoord, texCoord);

        return middle;
    }

    void GeometryGenerator::Subdivide(MeshData& meshData)
    {
        MeshData copy{ meshData };

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

        const auto triangleCount{ static_cast<uint32_t>(copy.Indices.size() / 3) };

        for (uint32_t i = 0; i < triangleCount; ++i)
        {
            const Vertex v0{ copy.Vertices[copy.Indices[i * 3 + 0]] };
            const Vertex v1{ copy.Vertices[copy.Indices[i * 3 + 1]] };
            const Vertex v2{ copy.Vertices[copy.Indices[i * 3 + 2]] };
            
            const Vertex middle0{ MiddlePoint(v0, v1) };
            const Vertex middle1{ MiddlePoint(v1, v2) };
            const Vertex middle2{ MiddlePoint(v0, v2) };

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

    void GeometryGenerator::GenerateCylinderTopCap(const CylinderGeometryConfig& props, MeshData& meshData)
    {
        const auto baseIndex{ static_cast<uint32_t>(meshData.Vertices.size()) };

        // Vertices
        {
            const float y{ 0.5f * props.Height };
            const float dTheta{ 2.0f * static_cast<float>(std::numbers::pi) / props.SliceCount };

            for (uint32_t i = 0; i <= props.SliceCount; ++i)
            {
                const float x{ props.TopRadius * std::cos(i * dTheta) };
                const float z{ props.TopRadius * std::sin(i * dTheta) };
                const float u{ x / props.Height + 0.5f };
                const float v{ z / props.Height + 0.5f };

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
            const auto centerIndex{ static_cast<uint32_t>(meshData.Vertices.size() - 1) };

            for (uint32_t i = 0; i < props.SliceCount; ++i)
            {
                meshData.Indices.push_back(centerIndex);
                meshData.Indices.push_back(baseIndex + i + 1);
                meshData.Indices.push_back(baseIndex + i);
            }
        }
    }

    void GeometryGenerator::GenerateCylinderBottomCap(const CylinderGeometryConfig& props, MeshData& meshData)
    {
        const auto baseIndex{ static_cast<uint32_t>(meshData.Vertices.size()) };

        // Vertices
        {
            const float y{ -0.5f * props.Height };
            const float dTheta{ DirectX::XM_2PI / props.SliceCount };

            for (uint32_t i = 0; i <= props.SliceCount; ++i)
            {
                const float x{ props.BottomRadius * cosf(i * dTheta) };
                const float z{ props.BottomRadius * sinf(i * dTheta) };
                const float u{ x / props.Height + 0.5f };
                const float v{ z / props.Height + 0.5f };
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
            const auto centerIndex{ static_cast<uint32_t>(meshData.Vertices.size() - 1) };

            for (uint32_t i = 0; i < props.SliceCount; ++i)
            {
                meshData.Indices.push_back(centerIndex);
                meshData.Indices.push_back(baseIndex + i);
                meshData.Indices.push_back(baseIndex + i + 1);
            }
        }
    }

} // namespace spieler
