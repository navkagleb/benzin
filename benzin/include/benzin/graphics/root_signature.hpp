#pragma once

#include "benzin/graphics/common.hpp"
#include "benzin/graphics/sampler.hpp"
#include "benzin/graphics/descriptor_manager.hpp"
#include "benzin/graphics/shader.hpp"

namespace benzin
{

    class RootParameter
    {
    private:
        BENZIN_NON_COPYABLE(RootParameter)

    private:
        friend class RootSignature;

    public:
        struct Constants32BitConfig
        {
            ShaderVisibility ShaderVisibility{ ShaderVisibility::All };
            Shader::Register ShaderRegister;
            uint32_t Count{ 0 };
        };

        struct DescriptorConfig
        {
            ShaderVisibility ShaderVisibility{ ShaderVisibility::All };
            Descriptor::Type Type{ Descriptor::Type::Unknown };
            Shader::Register ShaderRegister;
        };

        struct DescriptorRange
        {
            Descriptor::Type Type{ Descriptor::Type::Unknown };
            uint32_t DescriptorCount{ 0 };
            Shader::Register BaseShaderRegister;
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
        BENZIN_NON_COPYABLE(RootSignature)
        BENZIN_NON_MOVEABLE(RootSignature)
        BENZIN_DEBUG_NAME_D3D12_OBJECT(m_D3D12RootSignature, "RootSignature")

    public:
        struct Config
        {
            std::vector<RootParameter> RootParameters;
            std::vector<StaticSampler> StaticSamplers;
        };

    public:
        RootSignature() = default;
        RootSignature(Device& device, const Config& config, const char* debugName = nullptr);
        ~RootSignature();

    public:
        ID3D12RootSignature* GetD3D12RootSignature() const { return m_D3D12RootSignature; }

    private:
        ID3D12RootSignature* m_D3D12RootSignature{ nullptr };
    };


} // namespace benzin
