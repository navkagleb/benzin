#include "benzin/config/bootstrap.hpp"
#include "benzin/graphics/descriptor_manager.hpp"

#include "benzin/core/index_allocator.hpp"
#include "benzin/graphics/d3d12_utils.hpp"
#include "benzin/graphics/device.hpp"
#include "benzin/graphics/hr_assert.hpp"

namespace benzin
{
 
    static bool IsResourceDescriptor(DescriptorType descriptorType)
    {
        switch (descriptorType)
        {
            case DescriptorType::Rtv:
            case DescriptorType::Dsv:
            case DescriptorType::Sampler: return false;
            case DescriptorType::Cbv:
            case DescriptorType::Srv:
            case DescriptorType::Uav: return true;
        }

        std::unreachable();
    }

    // DescriptorHeap

    struct DescriptorHeapCreation
    {
        std::string_view DebugName;

        D3D12_DESCRIPTOR_HEAP_TYPE D3D12Type = D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; // Some invalid value
        uint32_t MaxDescriptorCount = 0;
        bool IsShaderVisible = false;
    };

    class DescriptorHeap
    {
    public:
        DescriptorHeap(Device& device, const DescriptorHeapCreation& creation)
            : m_IsShaderVisible{ creation.IsShaderVisible }
            , m_IndexAllocator{ creation.MaxDescriptorCount }
        {
            const D3D12_DESCRIPTOR_HEAP_DESC d3d12DescriptorHeapDesc
            {
                .Type = creation.D3D12Type,
                .NumDescriptors = creation.MaxDescriptorCount,
                .Flags = m_IsShaderVisible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE,
                .NodeMask = 0,
            };

            BenzinHrEnsure(device.GetD3D12Device()->CreateDescriptorHeap(&d3d12DescriptorHeapDesc, IID_PPV_ARGS(&m_D3D12DescriptorHeap)));
            SetDxObjectDebugName(m_D3D12DescriptorHeap, creation.DebugName);

            m_DescriptorSize = device.GetD3D12Device()->GetDescriptorHandleIncrementSize(d3d12DescriptorHeapDesc.Type);
        }

        ~DescriptorHeap()
        {
            BenzinWarningIf(
                m_IndexAllocator.GetAllocatedIndexCount() != 0,
                "Descriptor heap '{}' has {} allocated descriptors",
                GetDxObjectDebugName(m_D3D12DescriptorHeap),
                m_IndexAllocator.GetAllocatedIndexCount()
            );

            BenzinSafeDxObjectRelease(m_D3D12DescriptorHeap);
        }

        BenzinDefineNonCopyable(DescriptorHeap);
        BenzinDefineNonMoveable(DescriptorHeap);

    public:
        auto* GetD3D12DescriptorHeap() const { return m_D3D12DescriptorHeap; }

        uint32_t AllocateDescriptorIndex()
        {
            return m_IndexAllocator.AllocateIndex();
        }

        void FreeDescriptorIndex(uint32_t descriptorIndex)
        {
            m_IndexAllocator.FreeIndex(descriptorIndex);
        }

        uint64_t GetCpuHandle(uint32_t descriptorIndex) const
        {
            return m_D3D12DescriptorHeap->GetCPUDescriptorHandleForHeapStart().ptr + (uint64_t)descriptorIndex * m_DescriptorSize;
        }

        uint64_t GetGpuHandle(uint32_t descriptorIndex) const
        {
            BenzinAssert(m_IsShaderVisible, "GPU handle can only be obtained from shader visible heap");

            return m_D3D12DescriptorHeap->GetGPUDescriptorHandleForHeapStart().ptr + (uint64_t)descriptorIndex * m_DescriptorSize;
        }

        uint32_t GetDescriptorIndexByCpuHandle(uint64_t cpuHandle)
        {
            const auto descriptorIndex = (uint32_t)(cpuHandle - m_D3D12DescriptorHeap->GetCPUDescriptorHandleForHeapStart().ptr) / m_DescriptorSize;
            BenzinAssert(descriptorIndex < m_IndexAllocator.GetMaxIndexCount());

            return descriptorIndex;
        }

    private:
        ID3D12DescriptorHeap* m_D3D12DescriptorHeap = nullptr;

        bool m_IsShaderVisible = false;
        uint32_t m_DescriptorSize = 0;

        IndexAllocator m_IndexAllocator;
    };

    // DescriptorManager

    DescriptorManager::DescriptorManager(Device& device)
        : m_Device{ device }
    {
        MakeUniquePtr(m_CpuRtvHeap, m_Device, DescriptorHeapCreation
        {
            .DebugName = "DescriptorHeap_CpuRtv",
            .D3D12Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
            .MaxDescriptorCount = GfxConfig::s_MaxRtvDescriptorCount,
            .IsShaderVisible = false,
        });

        MakeUniquePtr(m_CpuDsvHeap, m_Device, DescriptorHeapCreation
        {
            .DebugName = "DescriptorHeap_CpuDsv",
            .D3D12Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV,
            .MaxDescriptorCount = GfxConfig::s_MaxDsvDescriptorCount,
            .IsShaderVisible = false,
        });

        MakeUniquePtr(m_CpuResourceHeap, m_Device, DescriptorHeapCreation
        {
            .DebugName = "DescriptorHeap_CpuResource",
            .D3D12Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
            .MaxDescriptorCount = GfxConfig::s_MaxResourceDescriptorCount,
            .IsShaderVisible = false,
        });

        MakeUniquePtr(m_GpuResourceHeap, m_Device, DescriptorHeapCreation
        {
            .DebugName = "DescriptorHeap_GpuResource",
            .D3D12Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
            .MaxDescriptorCount = GfxConfig::s_MaxResourceDescriptorCount,
            .IsShaderVisible = true,
        });
    }

    DescriptorManager::~DescriptorManager() = default;

    ID3D12DescriptorHeap* DescriptorManager::GetD3D12GpuResourceDescriptorHeap() const
    {
        return m_GpuResourceHeap->GetD3D12DescriptorHeap();
    }

    Descriptor DescriptorManager::AllocateDescriptor(DescriptorType descriptorType, const DescriptorInitCallback& initCallback)
    {
        Descriptor descriptor;
        descriptor.m_Type = descriptorType;

        auto& cpuHeap = GetCpuHeap(descriptorType);

        const uint32_t cpuDescriptorIndex = cpuHeap.AllocateDescriptorIndex();
        descriptor.m_CpuHandle = cpuHeap.GetCpuHandle(cpuDescriptorIndex);

        if (initCallback)
        {
            initCallback(descriptor.m_CpuHandle);
        }

        if (IsResourceDescriptor(descriptorType))
        {
            const uint32_t gpuDescriptorIndex = m_GpuResourceHeap->AllocateDescriptorIndex();

            descriptor.m_GpuHeapIndex = gpuDescriptorIndex;
            descriptor.m_GpuHandle = m_GpuResourceHeap->GetGpuHandle(gpuDescriptorIndex);

            if (initCallback)
            {
                // Copy only if CPU descriptor is initialized by callback
                CopyToGpuResourceHeap(descriptor);
            }
        }

        return descriptor;
    }

    void DescriptorManager::FreeDescriptor(const Descriptor& descriptor)
    {
        BenzinAssert(descriptor.IsCpuValid(), "Descriptor must already be initialized at least in CPU heap");

        auto& cpuHeap = GetCpuHeap(descriptor.GetType());
        cpuHeap.FreeDescriptorIndex(cpuHeap.GetDescriptorIndexByCpuHandle(descriptor.GetCpuHandle()));
        
        if (IsResourceDescriptor(descriptor.GetType()))
        {
            m_GpuResourceHeap->FreeDescriptorIndex(descriptor.GetGpuHeapIndex());
        }
    }

    void DescriptorManager::CopyToGpuResourceHeap(Descriptor& descriptor)
    {
        BenzinAssert(IsResourceDescriptor(descriptor.GetType()), "Descriptor must be in Resource heap");
        BenzinAssert(descriptor.IsGpuValid(), "Descriptor must already be initialized in CPU heap and GPU heap");

        m_Device.GetD3D12Device()->CopyDescriptorsSimple(
            1,
            D3D12_CPU_DESCRIPTOR_HANDLE{ m_GpuResourceHeap->GetCpuHandle(descriptor.GetGpuHeapIndex()) }, // Write only cpu handle
            D3D12_CPU_DESCRIPTOR_HANDLE{ descriptor.m_CpuHandle },
            D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV
        );
    }

    DescriptorHeap& DescriptorManager::GetCpuHeap(DescriptorType descriptorType)
    {
        switch (descriptorType)
        {
            case DescriptorType::Rtv: return *m_CpuRtvHeap;
            case DescriptorType::Dsv: return *m_CpuDsvHeap;
            case DescriptorType::Cbv:
            case DescriptorType::Srv:
            case DescriptorType::Uav: return *m_CpuResourceHeap;
        }

        std::unreachable();
    }

}
