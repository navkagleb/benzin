#pragma once

namespace spieler
{

    class Device;
    class Context;

    using CPUDescriptorHandle = uint64_t;
    using GPUDescriptorHandle = uint64_t;

#define _SPIELER_CPU_DESCRIPTOR(ClassName)                                                      \
    struct ClassName                                                                            \
    {                                                                                           \
        CPUDescriptorHandle CPU{ 0 };                                                           \
                                                                                                \
        explicit operator bool() const { return CPU != 0; }                                     \
    };

#define _SPIELER_CPU_GPU_DESCRIPTOR(ClassName)                                                  \
    struct ClassName                                                                            \
    {                                                                                           \
        uint32_t HeapIndex{ 0 };                                                                \
        CPUDescriptorHandle CPU{ 0 };                                                           \
        GPUDescriptorHandle GPU{ 0 };                                                           \
                                                                                                \
        explicit operator bool() const { return CPU != 0; }                                     \
    };

    _SPIELER_CPU_DESCRIPTOR(RTVDescriptor)
    _SPIELER_CPU_DESCRIPTOR(DSVDescriptor)
    _SPIELER_CPU_GPU_DESCRIPTOR(SamplerDescriptor)
    _SPIELER_CPU_GPU_DESCRIPTOR(CBVDescriptor)
    _SPIELER_CPU_GPU_DESCRIPTOR(SRVDescriptor)
    _SPIELER_CPU_GPU_DESCRIPTOR(UAVDescriptor)

//#undef _SPIELER_CPU_DESCRIPTOR
//#undef _SPIELER_CPU_GPU_DESCRIPTOR

    class DescriptorHeap
    {
    public:
        enum class Type : uint8_t
        {
            CBV_SRV_UAV,
            CBV = CBV_SRV_UAV,
            SRV = CBV_SRV_UAV,
            UAV = CBV_SRV_UAV,
            Sampler,
            RTV,
            DSV,
        };

    public:
        SPIELER_NON_COPYABLE(DescriptorHeap)

    public:
        friend class DescriptorManager;

    public:
        DescriptorHeap() = default;

    private:
        DescriptorHeap(Device& device, Type type, uint32_t descriptorCount);
        DescriptorHeap(DescriptorHeap&& other) noexcept;

    public:
        ID3D12DescriptorHeap* GetDX12DescriptorHeap() const { return m_DX12DescriptorHeap.Get(); }

        uint32_t GetDescriptorSize() const { return m_DescriptorSize; }
        uint32_t GetDescriptorCount() const { return m_DescriptorCount; }

    public:
        uint32_t AllocateIndex();
        CPUDescriptorHandle AllocateCPU(uint32_t index);
        GPUDescriptorHandle AllocateGPU(uint32_t index);

    private:
        bool Init(Device& device, Type type, uint32_t descriptorCount);

    private:
        DescriptorHeap& operator=(DescriptorHeap&& other) noexcept;

    private:
        ComPtr<ID3D12DescriptorHeap> m_DX12DescriptorHeap;

        uint32_t m_DescriptorSize{ 0 };
        uint32_t m_DescriptorCount{ 0 };
        uint32_t m_Marker{ 0 }; // TODO: Replace with allocator
    };

    class DescriptorManager
    {
    public:
        struct Config
        {
            uint32_t CBV_SRV_UAVDescriptorCount{ 0 };
            uint32_t SamplerDescriptorCount{ 0 };
            uint32_t RTVDescriptorCount{ 0 };
            uint32_t DSVDescriptorCount{ 0 };
        };

    public:
        SPIELER_NON_COPYABLE(DescriptorManager)

    public:
        DescriptorManager() = default;
        DescriptorManager(Device& device, const Config& config);
        DescriptorManager(DescriptorManager&& other) noexcept;

    public:
        const DescriptorHeap& GetDescriptorHeap(DescriptorHeap::Type type) const { return m_DescriptorHeaps.at(type); }

        RTVDescriptor AllocateRTV();
        DSVDescriptor AllocateDSV();
        SamplerDescriptor AllocateSampler();
        CBVDescriptor AllocateCBV();
        SRVDescriptor AllocateSRV();
        UAVDescriptor AllocateUAV();

    public:
        DescriptorManager& operator=(DescriptorManager&& other) noexcept;

    private:
        std::unordered_map<DescriptorHeap::Type, DescriptorHeap> m_DescriptorHeaps;
    };

} // namespace spieler
