#pragma once

#include <cstdint>
#include <vector>

#include <DirectXMath.h>

namespace sandbox
{

    struct WavesProps
    {
        uint32_t RowCount{ 0 };
        uint32_t ColumnCount{ 0 };
        float TimeStep{ 0.0f };
        float SpatialStep{ 0.0f };
        float Speed{ 0.0f };
        float Damping{ 0.0f };
    };

    class Waves
    {
    public:
        Waves(const WavesProps& props);

    public:
        uint32_t GetRowCount() const { return m_RowCount; }
        uint32_t GetColumnCount() const { return m_ColumnCount; }

        uint32_t GetVertexCount() const { return m_VertexCount; }
        uint32_t GetTriangleCount() const { return m_TriangleCount; }

        const DirectX::XMFLOAT3& GetPosition(uint32_t index) const { return m_CurrentSolution[index]; }
        const DirectX::XMFLOAT3& GetNormal(uint32_t index) const { return m_Normals[index]; }
        const DirectX::XMFLOAT3& GetTangentX(uint32_t index) const { return m_TangentX[index]; }

        float GetWidth() const { return m_ColumnCount * m_SpatialStep; }
        float GetDepth() const { return m_RowCount * m_SpatialStep; }

        void OnUpdate(float dt);

        void Disturb(uint32_t i, uint32_t j, float magnitude);

    private:
        uint32_t m_RowCount{ 0 };
        uint32_t m_ColumnCount{ 0 };

        uint32_t m_VertexCount{ 0 };
        uint32_t m_TriangleCount{ 0 };

        float m_K1{ 0.0f };
        float m_K2{ 0.0f };
        float m_K3{ 0.0f };

        float m_TimeStep{ 0.0f }; 
        float m_SpatialStep{ 0.0f };

        std::vector<DirectX::XMFLOAT3> m_PreviousSolution;
        std::vector<DirectX::XMFLOAT3> m_CurrentSolution;
        std::vector<DirectX::XMFLOAT3> m_Normals;
        std::vector<DirectX::XMFLOAT3> m_TangentX;
    };

} // namespace sandbox