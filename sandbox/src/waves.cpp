#include "bootstrap.hpp"

#include "waves.hpp"

#include <ppl.h>

#include <algorithm>

#include <benzin/core/assert.hpp>
#include <benzin/core/common.hpp>

namespace sandbox
{

    Waves::Waves(const WavesProps& props)
        : m_RowCount(props.RowCount)
        , m_ColumnCount(props.ColumnCount)
        , m_VertexCount(props.RowCount * props.ColumnCount)
        , m_TriangleCount((props.RowCount - 1) * (props.ColumnCount - 1) * 2)
        , m_TimeStep(props.TimeStep)
        , m_SpatialStep(props.SpatialStep)
    {
        const float d{ props.Damping * m_TimeStep + 2.0f };
        const float e{ (props.Speed * props.Speed) * (m_TimeStep * m_TimeStep) / (m_SpatialStep * m_SpatialStep) };

        m_K1 = (props.Damping * m_TimeStep - 2.0f) / d;
        m_K2 = (4.0f - 8.0f * e) / d;
        m_K3 = (2.0f * e) / d;

        m_PreviousSolution.resize(m_VertexCount);
        m_CurrentSolution.resize(m_VertexCount);
        m_Normals.resize(m_VertexCount);
        m_TangentX.resize(m_VertexCount);

        const float halfWidth{ (m_RowCount - 1) * m_SpatialStep * 0.5f };
        const float halfDepth{ (m_ColumnCount - 1) * m_SpatialStep * 0.5f };

        for (uint32_t i = 0; i < m_RowCount; ++i)
        {
            const float z = halfDepth - i * m_SpatialStep;

            for (uint32_t j = 0; j < m_ColumnCount; ++j)
            {
                const float x = -halfWidth + j * m_SpatialStep;

                m_PreviousSolution[i * m_ColumnCount + j] = DirectX::XMFLOAT3{ x, 0.0f, z };
                m_CurrentSolution[i * m_ColumnCount + j] = DirectX::XMFLOAT3{ x, 0.0f, z };
                m_Normals[i * m_ColumnCount + j] = DirectX::XMFLOAT3{ 0.0f, 1.0f, 0.0f };
                m_TangentX[i * m_ColumnCount + j] = DirectX::XMFLOAT3{ 1.0f, 0.0f, 0.0f };
            }
        }
    }

    void Waves::OnUpdate(float dt)
    {
        static float t{ 0.0f };

		t += dt;

		if (t >= m_TimeStep)
		{
			concurrency::parallel_for<uint32_t>(1, m_RowCount - 1, [this](uint32_t i)
			{
				for (uint32_t j = 1; j < m_ColumnCount - 1; ++j)
				{
					m_PreviousSolution[i * m_ColumnCount + j].y =
						m_K1 * m_PreviousSolution[i * m_ColumnCount + j].y +
						m_K2 * m_CurrentSolution[i * m_ColumnCount + j].y +
						m_K3 * (m_CurrentSolution[(i + 1) * m_ColumnCount + j].y +
						m_CurrentSolution[(i - 1) * m_ColumnCount + j].y +
						m_CurrentSolution[i * m_ColumnCount + j + 1].y +
						m_CurrentSolution[i * m_ColumnCount + j - 1].y);
				}
			});

			std::swap(m_PreviousSolution, m_CurrentSolution);

			t = 0.0f;

			concurrency::parallel_for<uint32_t>(1, m_RowCount - 1, [this](uint32_t i)
			{
				for (uint32_t j = 1; j < m_ColumnCount - 1; ++j)
				{
                    const float l{ m_CurrentSolution[i * m_ColumnCount + j - 1].y };
                    const float r{ m_CurrentSolution[i * m_ColumnCount + j + 1].y };
                    const float t{ m_CurrentSolution[(i - 1) * m_ColumnCount + j].y };
                    const float b{ m_CurrentSolution[(i + 1) * m_ColumnCount + j].y };

					m_Normals[i * m_ColumnCount + j].x = -r + l;
					m_Normals[i * m_ColumnCount + j].y = 2.0f * m_SpatialStep;
					m_Normals[i * m_ColumnCount + j].z = b - t;

					DirectX::XMVECTOR normal = DirectX::XMVector3Normalize(DirectX::XMLoadFloat3(&m_Normals[i * m_ColumnCount + j]));
					DirectX::XMStoreFloat3(&m_Normals[i * m_ColumnCount + j], normal);

					m_TangentX[i * m_ColumnCount + j] = DirectX::XMFLOAT3(2.0f * m_SpatialStep, r - l, 0.0f);

					DirectX::XMVECTOR tangentX = DirectX::XMVector3Normalize(DirectX::XMLoadFloat3(&m_TangentX[i * m_ColumnCount + j]));
					DirectX::XMStoreFloat3(&m_TangentX[i * m_ColumnCount + j], tangentX);
				}
			});
		}
    }

    void Waves::Disturb(uint32_t i, uint32_t j, float magnitude)
    {
        BENZIN_ASSERT(i > 1 && i < m_RowCount - 2);
        BENZIN_ASSERT(j > 1 && j < m_ColumnCount - 2);

        const float halfMagnitude{ 0.5f * magnitude };

        m_CurrentSolution[i * m_ColumnCount + j].y += magnitude;
        m_CurrentSolution[i * m_ColumnCount + j + 1].y += halfMagnitude;
        m_CurrentSolution[i * m_ColumnCount + j - 1].y += halfMagnitude;
        m_CurrentSolution[(i + 1) * m_ColumnCount + j].y += halfMagnitude;
        m_CurrentSolution[(i - 1) * m_ColumnCount + j].y += halfMagnitude;
    }

} // namespace sandbox