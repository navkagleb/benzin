#pragma once

namespace benzin
{

    // Forward declaration of resource ids

    enum class BufferId : uint32_t;
    enum class PsoId : uint32_t;
    enum class TextureId : uint32_t;

    enum class ComparisonFunction : std::underlying_type_t<D3D12_COMPARISON_FUNC>
    {
        Never = D3D12_COMPARISON_FUNC_NEVER,
        Less = D3D12_COMPARISON_FUNC_LESS,
        Equal = D3D12_COMPARISON_FUNC_EQUAL,
        LessEqual = D3D12_COMPARISON_FUNC_LESS_EQUAL,
        Greater = D3D12_COMPARISON_FUNC_GREATER,
        NotEqual = D3D12_COMPARISON_FUNC_NOT_EQUAL,
        GreaterEqual = D3D12_COMPARISON_FUNC_GREATER_EQUAL,
        Always = D3D12_COMPARISON_FUNC_ALWAYS,
    };

}
