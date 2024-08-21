#include "benzin/config/bootstrap.hpp"
#include "benzin/graphics/descriptor_manager.hpp"

#include "benzin/core/asserter.hpp"
#include "benzin/core/logger.hpp"
#include "benzin/graphics/d3d12_utils.hpp"
#include "benzin/graphics/device.hpp"

namespace benzin
{
 
    BenzinEnableUnaryPlusForEnum(DescriptorType);

    static D3D12_DESCRIPTOR_HEAP_TYPE GetDescriptorHeapType(DescriptorType descripitorType)
    {
        switch (descripitorType)
        {
            using enum DescriptorType;

            case Rtv: return D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
            case Dsv: return D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
            case Cbv:
            case Srv:
            case Uav: return D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
            case Sampler: return D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
        }

        std::unreachable();
    }

    // DescriptorHeap

    class DescriptorHeap
    {
    public:
        DescriptorHeap(Device& device, D3D12_DESCRIPTOR_HEAP_TYPE d3d12DescriptorHeapType, uint32_t descriptorCount);
        ~DescriptorHeap();

        BenzinDefineNonCopyable(DescriptorHeap);
        BenzinDefineNonMoveable(DescriptorHeap);

    public:
        auto* GetD3D12DescriptorHeap() const { return m_D3D12DescriptorHeap; }

        Descriptor AllocateDescriptor(DescriptorType type);
        void FreeDescriptor(const Descriptor& descriptor);

    private:
        uint32_t AllocateIndex();
        void FreeIndex(uint32_t index);

        uint64_t GetCpuHandle(uint32_t index) const;
        uint64_t GetGpuHandle(uint32_t index) const;

    private:
        ID3D12DescriptorHeap* m_D3D12DescriptorHeap = nullptr;

        bool m_IsAccessableByShader = false;
        uint32_t m_DescriptorSize = 0;
        uint32_t m_DescriptorCount = 0;

        uint32_t m_Marker = 0;
        std::list<uint32_t> m_FreeIndices;

#if BENZIN_IS_DEBUG_BUILD
        EnumArray<uint32_t, DescriptorType> m_AllocatedIndexCounts{};
#endif
    };

    DescriptorHeap::DescriptorHeap(Device& device, D3D12_DESCRIPTOR_HEAP_TYPE d3d12DescriptorHeapType, uint32_t descriptorCount)
        : m_IsAccessableByShader{ d3d12DescriptorHeapType == D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV || d3d12DescriptorHeapType == D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER }
        , m_DescriptorCount{ descriptorCount }
    {
        const D3D12_DESCRIPTOR_HEAP_DESC d3d12DescriptorHeapDesc
        {
            .Type = d3d12DescriptorHeapType,
            .NumDescriptors = descriptorCount,
            .Flags = m_IsAccessableByShader ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE,
            .NodeMask = 0,
        };

        BenzinEnsure(device.GetD3D12Device()->CreateDescriptorHeap(&d3d12DescriptorHeapDesc, IID_PPV_ARGS(&m_D3D12DescriptorHeap)));
        SetDxObjectDebugName(m_D3D12DescriptorHeap, magic_enum::enum_name(d3d12DescriptorHeapType));

        m_DescriptorSize = device.GetD3D12Device()->GetDescriptorHandleIncrementSize(d3d12DescriptorHeapDesc.Type);
    }

    DescriptorHeap::~DescriptorHeap()
    {
#if BENZIN_IS_DEBUG_BUILD
        for (const auto [i, count] : m_AllocatedIndexCounts | std::views::enumerate)
        {
            BenzinWarningIf(count != 0, "Remainding allocated index count {} for DescriptorType::{}", count, magic_enum::enum_name((DescriptorType)i));
        }
#endif

        BenzinSafeDxObjectRelease(m_D3D12DescriptorHeap);
    }

    Descriptor DescriptorHeap::AllocateDescriptor(DescriptorType type)
    {
        const uint32_t heapIndex = AllocateIndex();

#if BENZIN_IS_DEBUG_BUILD
        m_AllocatedIndexCounts[+type]++;
#endif

        return Descriptor{ type, heapIndex, GetCpuHandle(heapIndex), m_IsAccessableByShader ? GetGpuHandle(heapIndex) : 0 };
    }

    void DescriptorHeap::FreeDescriptor(const Descriptor& descriptor)
    {
        FreeIndex(descriptor.GetHeapIndex());

#if BENZIN_IS_DEBUG_BUILD
        m_AllocatedIndexCounts[+descriptor.GetType()]--;
#endif
    }

    uint32_t DescriptorHeap::AllocateIndex()
    {
        auto index = g_InvalidUnsigned<uint32_t>;

        if (!m_FreeIndices.empty())
        {
            index = m_FreeIndices.back();
            m_FreeIndices.pop_back();
        }
        else
        {
            index = m_Marker++;
        }

        BenzinAssert(IsValidUnsigned(index));
        return index;
    }

    void DescriptorHeap::FreeIndex(uint32_t index)
    {
        BenzinAssert(std::find(m_FreeIndices.begin(), m_FreeIndices.end(), index) == m_FreeIndices.end());
        m_FreeIndices.push_back(index);
    }

    uint64_t DescriptorHeap::GetCpuHandle(uint32_t index) const
    {
        return m_D3D12DescriptorHeap->GetCPUDescriptorHandleForHeapStart().ptr + (uint64_t)index * m_DescriptorSize;
    }

    uint64_t DescriptorHeap::GetGpuHandle(uint32_t index) const
    {
        return m_D3D12DescriptorHeap->GetGPUDescriptorHandleForHeapStart().ptr + (uint64_t)index * m_DescriptorSize;
    }

    // DescriptorManager

    DescriptorManager::DescriptorManager(Device& device)
    {
        const auto& createDescriptorHeap = [&](D3D12_DESCRIPTOR_HEAP_TYPE d3d12DescriptorHeapType, uint32_t descriptorCount)
        {
            MakeUniquePtr(m_DescriptorHeaps[magic_enum::enum_integer(d3d12DescriptorHeapType)], device, d3d12DescriptorHeapType, descriptorCount);
        };

        createDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_RTV, GfxConfig::s_MaxRtvDescriptorCount);
        createDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_DSV, GfxConfig::s_MaxDsvDescriptorCount);
        createDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, GfxConfig::s_MaxResourceDescriptorCount);
        createDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, GfxConfig::s_MaxSamplerDescriptorCount);
    }

    DescriptorManager::~DescriptorManager() = default;

    ID3D12DescriptorHeap* DescriptorManager::GetD3D12GpuResourceDescriptorHeap() const
    {
        return m_DescriptorHeaps.at(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV)->GetD3D12DescriptorHeap();
    }

    ID3D12DescriptorHeap* DescriptorManager::GetD3D12SamplerDescriptorHeap() const
    {
        return m_DescriptorHeaps.at(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER)->GetD3D12DescriptorHeap();
    }

    Descriptor DescriptorManager::AllocateDescriptor(DescriptorType descriptorType)
    {
        DescriptorHeap& descriptorHeap = *m_DescriptorHeaps[GetDescriptorHeapType(descriptorType)];
        return descriptorHeap.AllocateDescriptor(descriptorType);
    }

    void DescriptorManager::FreeDescriptor(const Descriptor& descriptor)
    {
        DescriptorHeap& descriptorHeap = *m_DescriptorHeaps[GetDescriptorHeapType(descriptor.GetType())];
        descriptorHeap.FreeDescriptor(descriptor);
    }

} // namespace benzin
