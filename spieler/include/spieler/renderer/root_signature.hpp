#pragma once

#include <vector>
#include <variant>

#include "bindable.hpp"
#include "shader_visibility.hpp"

namespace Spieler
{

    struct StaticSampler;

    enum RootParameterType : std::int32_t
    {
        RootParameterType_Undefined = -1,

        RootParameterType_DescriptorTable = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE,
        RootParameterType_32BitConstants = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS,
        RootParameterType_ConstantBufferView = D3D12_ROOT_PARAMETER_TYPE_CBV,
        RootParameterType_ShaderResourceView = D3D12_ROOT_PARAMETER_TYPE_SRV,
        RootParameterType_UnorderedAccessView = D3D12_ROOT_PARAMETER_TYPE_UAV
    };

    enum DescriptorRangeType : std::int32_t
    {
        DescriptorRangeType_Undefined = -1,

        DescriptorRangeType_SRV = D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
        DescriptorRangeType_UAV = D3D12_DESCRIPTOR_RANGE_TYPE_UAV,
        DescriptorRangeType_CBV = D3D12_DESCRIPTOR_RANGE_TYPE_CBV,
        DescriptorRangeType_Sampler = D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER
    };

    struct DescriptorRange
    {
        DescriptorRangeType Type{ DescriptorRangeType_Undefined };
        std::uint32_t DescriptorCount{ 0 };
        std::uint32_t BaseShaderRegister{ 0 };
        std::uint32_t RegisterSpace{ 0 };
        std::uint32_t Offset{ 0xffffffff };
    };

    struct RootDescriptorTable
    {
        std::vector<DescriptorRange> DescriptorRanges;
    };

    struct RootConstants
    {
        std::uint32_t ShaderRegister{ 0 };
        std::uint32_t RegisterSpace{ 0 };
        std::uint32_t Count{ 0 };
    };

    struct RootDescriptor
    {
        std::uint32_t ShaderRegister{ 0 };
        std::uint32_t RegisterSpace{ 0 };
    };

    using RootParameterChild = std::variant<RootDescriptorTable, RootConstants, RootDescriptor>;

    struct RootParameter
    {
        RootParameterType Type{ RootParameterType_Undefined };
        ShaderVisibility ShaderVisibility{ ShaderVisibility_All };
        RootParameterChild Child;
    };

    class RootSignature : public Bindable
    {
    public:
        bool Init(const std::vector<RootParameter>& rootParameters);
        bool Init(const std::vector<RootParameter>& rootParameters, const std::vector<StaticSampler>& staticSamplers);

        void Bind() const override;

    private:
        bool Init(const D3D12_ROOT_SIGNATURE_DESC& rootSignatureDesc);

    public:
        explicit operator ID3D12RootSignature* () const { return m_Handle.Get(); }

    private:
        ComPtr<ID3D12RootSignature> m_Handle;
    };


} // namespace Spieler