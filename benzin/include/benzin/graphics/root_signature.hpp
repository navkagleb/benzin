#pragma once

#include "benzin/graphics/common.hpp"
#include "benzin/graphics/sampler.hpp"
#include "benzin/graphics/descriptor_manager.hpp"

namespace benzin
{

    class RootParameter
    {
    private:
        BENZIN_NON_COPYABLE(RootParameter)

    private:
        friend class RootSignature;

    public:
        struct ShaderRegister
        {
            uint32_t Register{ 0 };
            uint32_t Space{ 0 };
        };

        struct Constants32BitConfig
        {
            ShaderVisibility ShaderVisibility{ ShaderVisibility::All };
            ShaderRegister ShaderRegister;
            uint32_t Count{ 0 };
        };

        struct DescriptorConfig
        {
            ShaderVisibility ShaderVisibility{ ShaderVisibility::All };
            Descriptor::Type Type{ Descriptor::Type::Unknown };
            ShaderRegister ShaderRegister;
        };

        struct DescriptorRange
        {
            Descriptor::Type Type{ Descriptor::Type::Unknown };
            uint32_t DescriptorCount{ 0 };
            ShaderRegister BaseShaderRegister;
        };

        struct DescriptorTableConfig
        {
            ShaderVisibility ShaderVisibility{ ShaderVisibility::All };
            std::vector<DescriptorRange> Ranges;
        };

        struct SingleDescriptorTableConfig
        {
            ShaderVisibility ShaderVisibility{ ShaderVisibility::All };
            DescriptorRange Range;
        };

    public:
        RootParameter() = default;
        RootParameter(const Constants32BitConfig& constants32BitConfig);
        RootParameter(const DescriptorConfig& descriptorConfig);
        RootParameter(const DescriptorTableConfig& descriptorTableConfig);
        RootParameter(const SingleDescriptorTableConfig& singleDescriptorTableConfig);
        RootParameter(RootParameter&& other) noexcept;
        ~RootParameter();

    private:
        void InitAsConstants32Bit(const Constants32BitConfig& constants32BitConfig);
        void InitAsDescriptor(const DescriptorConfig& descriptorConfig);
        void InitAsDescriptorTable(const DescriptorTableConfig& descriptorTableConfig);
        void InitAsSingleDescriptorTable(const SingleDescriptorTableConfig& singleDescriptorTableConfig);

        void Swap(RootParameter&& other) noexcept;

    public:
        RootParameter& operator=(RootParameter&& other) noexcept;

    private:
        D3D12_ROOT_PARAMETER m_D3D12RootParameter;
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
        BENZIN_NON_COPYABLE(RootSignature)

    public:
        RootSignature() = default;
        RootSignature(Device& device, const Config& config);
        RootSignature(RootSignature&& other) noexcept;

    public:
        ID3D12RootSignature* GetD3D12RootSignature() const { return m_D3D12RootSignature.Get(); }

    public:
        RootSignature& operator=(RootSignature&& other) noexcept;

    private:
        ComPtr<ID3D12RootSignature> m_D3D12RootSignature;
    };


} // namespace benzin
