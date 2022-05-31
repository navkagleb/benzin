#pragma once

#include "spieler/renderer/types.hpp"

namespace spieler::renderer
{

    struct StaticSampler;

    enum class RootParameterType : int32_t
    {
        Undefined = -1,

        DescriptorTable = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE,
        _32BitConstants = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS,
        ConstantBufferView = D3D12_ROOT_PARAMETER_TYPE_CBV,
        ShaderResourceView = D3D12_ROOT_PARAMETER_TYPE_SRV,
        UnorderedAccessView = D3D12_ROOT_PARAMETER_TYPE_UAV
    };

    enum class DescriptorRangeType : int32_t
    {
        Undefined = -1,

        SRV = D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
        UAV = D3D12_DESCRIPTOR_RANGE_TYPE_UAV,
        CBV = D3D12_DESCRIPTOR_RANGE_TYPE_CBV,
        Sampler = D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER
    };

    struct DescriptorRange
    {
        DescriptorRangeType Type{ DescriptorRangeType::Undefined };
        uint32_t DescriptorCount{ 0 };
        uint32_t BaseShaderRegister{ 0 };
        uint32_t RegisterSpace{ 0 };
        uint32_t Offset{ 0xffffffff };
    };

    struct RootDescriptorTable
    {
        std::vector<DescriptorRange> DescriptorRanges;
    };

    struct RootConstants
    {
        uint32_t ShaderRegister{ 0 };
        uint32_t RegisterSpace{ 0 };
        uint32_t Count{ 0 };
    };

    struct RootDescriptor
    {
        uint32_t ShaderRegister{ 0 };
        uint32_t RegisterSpace{ 0 };
    };

    using RootParameterChild = std::variant<RootDescriptorTable, RootConstants, RootDescriptor>;

    struct RootParameter
    {
        RootParameterType Type{ RootParameterType::Undefined };
        ShaderVisibility ShaderVisibility{ ShaderVisibility::All };
        RootParameterChild Child;
    };

    class RootSignature
    {
    public:
        bool Init(const std::vector<RootParameter>& rootParameters);
        bool Init(const std::vector<RootParameter>& rootParameters, const std::vector<StaticSampler>& staticSamplers);

        void Bind() const;

    private:
        bool Init(const D3D12_ROOT_SIGNATURE_DESC& rootSignatureDesc);

    public:
        explicit operator ID3D12RootSignature* () const { return m_Handle.Get(); }

    private:
        ComPtr<ID3D12RootSignature> m_Handle;
    };


} // namespace spieler::renderer