#pragma once

#include "spieler/renderer/types.hpp"

namespace spieler::renderer
{
    class Device;
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

    class Device;

    class RootSignature
    {
    private:
        SPIELER_NON_COPYABLE(RootSignature)

    public:
        RootSignature() = default;
        RootSignature(class Device& device, const std::vector<RootParameter>& rootParameters);
        RootSignature(class Device& device, const std::vector<RootParameter>& rootParameters, const std::vector<StaticSampler>& staticSamplers);

        RootSignature(RootSignature&& other) noexcept;

    private:
        bool Init(class Device& device, const std::vector<RootParameter>& rootParameters);
        bool Init(class Device& device, const std::vector<RootParameter>& rootParameters, const std::vector<StaticSampler>& staticSamplers);

        bool Init(class Device& device, const D3D12_ROOT_SIGNATURE_DESC& rootSignatureDesc);

    public:
        RootSignature& operator=(RootSignature&& other) noexcept;

        explicit operator ID3D12RootSignature* () const { return m_RootSignature.Get(); }

    private:
        ComPtr<ID3D12RootSignature> m_RootSignature;
    };


} // namespace spieler::renderer
