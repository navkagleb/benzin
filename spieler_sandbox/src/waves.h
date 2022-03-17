#pragma once

#include <cstdint>
#include <vector>

#include <DirectXMath.h>

namespace Sandbox
{

    struct WavesProps
    {
        std::uint32_t   RowCount{ 0 };
        std::uint32_t   ColumnCount{ 0 };
        float           TimeStep{ 0.0f };
        float           SpatialStep{ 0.0f };
        float           Speed{ 0.0f };
        float           Damping{ 0.0f };
    };

    class Waves
    {
    public:
        Waves(const WavesProps& props);

    public:
        std::uint32_t GetRowCount() const { return m_RowCount; }
        std::uint32_t GetColumnCount() const { return m_ColumnCount; }

        std::uint32_t GetVertexCount() const { return m_VertexCount; }
        std::uint32_t GetTriangleCount() const { return m_TriangleCount; }

        const DirectX::XMFLOAT3& GetPosition(std::uint32_t index) const { return m_CurrentSolution[index]; }
        const DirectX::XMFLOAT3& GetNormal(std::uint32_t index) const { return m_Normals[index]; }
        const DirectX::XMFLOAT3& GetTangentX(std::uint32_t index) const { return m_TangentX[index]; }

        void OnUpdate(float dt);

        void Disturb(std::uint32_t i, std::uint32_t j, float magnitude);

    private:
        std::uint32_t                   m_RowCount{ 0 };
        std::uint32_t                   m_ColumnCount{ 0 };

        std::uint32_t                   m_VertexCount{ 0 };
        std::uint32_t                   m_TriangleCount{ 0 };

        float                           m_K1{ 0.0f };
        float                           m_K2{ 0.0f };
        float                           m_K3{ 0.0f };

        float                           m_TimeStep{ 0.0f }; 
        float                           m_SpatialStep{ 0.0f };

        std::vector<DirectX::XMFLOAT3>  m_PreviousSolution;
        std::vector<DirectX::XMFLOAT3>  m_CurrentSolution;
        std::vector<DirectX::XMFLOAT3>  m_Normals;
        std::vector<DirectX::XMFLOAT3>  m_TangentX;
    };

} // namespace Sandbox