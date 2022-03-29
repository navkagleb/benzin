#include "renderer/sampler.hpp"

#include "renderer/descriptor_heap.hpp"

namespace Spieler
{

    namespace _Internal
    {

        D3D12_FILTER ToD3D12TextureFilter(const TextureFilter& filter)
        {
            const TextureFilterType minification{ filter.Minification };
            const TextureFilterType magnification{ filter.Magnification };
            const TextureFilterType mipLevel{ filter.MipLevel };

            D3D12_FILTER d3d12Filter{ D3D12_FILTER_MIN_MAG_MIP_POINT };

            if (minification == TextureFilterType_Point)
            {
                SPIELER_ASSERT(magnification != TextureFilterType_Anisotropic && mipLevel != TextureFilterType_Anisotropic);

                if (magnification == TextureFilterType_Point)
                {
                    d3d12Filter = mipLevel == TextureFilterType_Point ? D3D12_FILTER_MIN_MAG_MIP_POINT : D3D12_FILTER_MIN_MAG_POINT_MIP_LINEAR;
                }
                else
                {
                    d3d12Filter = mipLevel == TextureFilterType_Point ? D3D12_FILTER_MIN_POINT_MAG_LINEAR_MIP_POINT : D3D12_FILTER_MIN_POINT_MAG_MIP_LINEAR;
                }
            }
            else if (minification == TextureFilterType_Linear)
            {
                SPIELER_ASSERT(magnification != TextureFilterType_Anisotropic && mipLevel != TextureFilterType_Anisotropic);

                if (magnification == TextureFilterType_Point)
                {
                    d3d12Filter = mipLevel == TextureFilterType_Point ? D3D12_FILTER_MIN_LINEAR_MAG_MIP_POINT : D3D12_FILTER_MIN_LINEAR_MAG_POINT_MIP_LINEAR;
                }
                else
                {
                    d3d12Filter = mipLevel == TextureFilterType_Point ? D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT : D3D12_FILTER_MIN_MAG_MIP_LINEAR;
                }
            }
            else if (minification == TextureFilterType_Anisotropic)
            {
                SPIELER_ASSERT(magnification == TextureFilterType_Anisotropic && mipLevel == TextureFilterType_Anisotropic);

                d3d12Filter = D3D12_FILTER_ANISOTROPIC;
            }

            return d3d12Filter;
        }

    } // namespace _Internal

    Sampler::Sampler(const SamplerProps& props, const DescriptorHeap& descriptorHeap, std::uint32_t descriptorHeapIndex)
    {
        Init(props, descriptorHeap, descriptorHeapIndex);
    }

    void Sampler::Init(const SamplerProps& props, const DescriptorHeap& descriptorHeap, std::uint32_t descriptorHeapIndex)
    {
        m_Props = props;
        m_DescriptorHeap = &descriptorHeap;
        m_DescriptorHeapIndex = descriptorHeapIndex;

        D3D12_SAMPLER_DESC samplerDesc{};
        samplerDesc.Filter = _Internal::ToD3D12TextureFilter(m_Props.TextureFilter);
        samplerDesc.AddressU = static_cast<D3D12_TEXTURE_ADDRESS_MODE>(props.AddressU);
        samplerDesc.AddressV = static_cast<D3D12_TEXTURE_ADDRESS_MODE>(props.AddressV);
        samplerDesc.AddressW = static_cast<D3D12_TEXTURE_ADDRESS_MODE>(props.AddressW);
        samplerDesc.MipLODBias = 0.0f;
        samplerDesc.MaxAnisotropy = 1.0f;
        samplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
        samplerDesc.MinLOD = 0.0f;
        samplerDesc.MaxLOD = D3D12_FLOAT32_MAX;
        GetDevice()->CreateSampler(&samplerDesc, D3D12_CPU_DESCRIPTOR_HANDLE{ m_DescriptorHeap->GetDescriptorCPUHandle(m_DescriptorHeapIndex) });
    }

    void Sampler::Bind(std::uint32_t rootParameterIndex) const
    {
        SPIELER_ASSERT(m_DescriptorHeap);

        GetCommandList()->SetGraphicsRootDescriptorTable(rootParameterIndex, D3D12_GPU_DESCRIPTOR_HANDLE{ m_DescriptorHeap->GetDescriptorGPUHandle(m_DescriptorHeapIndex) });
    }

} // namespace Spieler