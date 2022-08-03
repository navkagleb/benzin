#pragma once

#include "spieler/renderer/common.hpp"
#include "spieler/renderer/sampler.hpp"

namespace spieler::renderer
{

    class Device;

    class RootParameter
    {
    private:
        SPIELER_NON_COPYABLE(RootParameter)

    private:
        friend class RootSignature;

    public:
        enum class DescriptorType : uint8_t
        {
            Unknown,

            ConstantBufferView,
            ShaderResourceView,
            UnorderedAccessView
        };

        enum class DescriptorRangeType : uint8_t
        {
            Unknown,

            ShaderResourceView,
            UnorderedAccessView,
            ConstantBufferView,
        };

        struct ShaderRegister
        {
            uint32_t Register{ 0 };
            uint32_t Space{ 0 };
        };

        struct _32BitConstants
        {
            ShaderVisibility ShaderVisibility{ ShaderVisibility::All };
            ShaderRegister ShaderRegister;
            uint32_t Count{ 0 };
        };

        struct Descriptor
        {
            ShaderVisibility ShaderVisibility{ ShaderVisibility::All };
            DescriptorType Type{ DescriptorType::Unknown };
            ShaderRegister ShaderRegister;
        };

        struct DescriptorRange
        {
            DescriptorRangeType Type{ DescriptorRangeType::Unknown };
            uint32_t DescriptorCount{ 0 };
            ShaderRegister BaseShaderRegister;
        };

        struct DescriptorTable
        {
            ShaderVisibility ShaderVisibility{ ShaderVisibility::All };
            std::vector<DescriptorRange> Ranges;
        };

        struct SingleDescriptorTable
        {
            ShaderVisibility ShaderVisibility{ ShaderVisibility::All };
            DescriptorRange Range;
        };

    public:
        RootParameter() = default;
        RootParameter(const _32BitConstants& constants);
        RootParameter(const Descriptor& descriptor);
        RootParameter(const DescriptorTable& descriptorTable);
        RootParameter(const SingleDescriptorTable& singleDescriptorTable);
        RootParameter(RootParameter&& other) noexcept;
        ~RootParameter();

    private:
        void InitAs32BitConstants(const _32BitConstants& constants);
        void InitAsDescriptor(const Descriptor& descriptor);
        void InitAsDescriptorTable(const DescriptorTable& descriptorTable);
        void InitAsSingleDescriptorTable(const SingleDescriptorTable& singleDescriptorTable);

        void Swap(RootParameter&& other) noexcept;

    public:
        RootParameter& operator=(RootParameter&& other) noexcept;

    private:
        D3D12_ROOT_PARAMETER m_DX12RootParameter;
    };

    class RootSignature
    {
    public:
        struct Config
        {
            std::vector<RootParameter> RootParameters;
            std::vector<StaticSampler> StaticSamplers;

            Config() = default;
            Config(size_t rootParametersSize, size_t staticSamplersSize = 0)
            {
                RootParameters.resize(rootParametersSize);
                StaticSamplers.resize(staticSamplersSize);
            }
        };

    private:
        SPIELER_NON_COPYABLE(RootSignature)

    public:
        RootSignature() = default;
        RootSignature(Device& device, const Config& config);
        RootSignature(RootSignature&& other) noexcept;

    public:
        ID3D12RootSignature* GetDX12RootSignature() const { return m_DX12RootSignature.Get(); }

    private:
        bool Init(class Device& device, const Config& config);

    public:
        RootSignature& operator=(RootSignature&& other) noexcept;

    private:
        ComPtr<ID3D12RootSignature> m_DX12RootSignature;
    };


} // namespace spieler::renderer
