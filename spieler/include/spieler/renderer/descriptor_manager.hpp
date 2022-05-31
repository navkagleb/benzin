#pragma once

namespace spieler::renderer
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

    struct DescriptorManagerConfig
    {
        uint32_t RTVDescriptorCount{ 0 };
        uint32_t DSVDescriptorCount{ 0 };
        uint32_t SamplerDescriptorCount{ 0 };
        uint32_t CBVDescriptorCount{ 0 };
        uint32_t SRVDescriptorCount{ 0 };
        uint32_t UAVDescriptorCount{ 0 };
    };

    enum class DescriptorHeapType : uint8_t
    {
        RTV = D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
        DSV = D3D12_DESCRIPTOR_HEAP_TYPE_DSV,
        Sampler = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER,
        CBV = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
        SRV = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
        UAV = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV
    };

    class DescriptorManager
    {
    private:
        class DescriptorHeap
        {
        public:
            DescriptorHeap() = default;
            DescriptorHeap(Device& device, DescriptorHeapType type, uint32_t descriptorCount);

        public:
            uint32_t GetDescriptorSize() const { return m_DescriptorSize; }
            uint32_t GetDescriptorCount() const { return m_DescriptorCount; }

        public:
            const ComPtr<ID3D12DescriptorHeap>& GetNative() const { return m_DescriptorHeap; }

            uint32_t AllocateIndex();
            CPUDescriptorHandle AllocateCPU(uint32_t index);
            GPUDescriptorHandle AllocateGPU(uint32_t index);

            void Bind(Context& context) const;

        private:
            bool Init(Device& device, DescriptorHeapType type, uint32_t descriptorCount);

        private:
            ComPtr<ID3D12DescriptorHeap> m_DescriptorHeap;
            uint32_t m_DescriptorSize{ 0 };
            uint32_t m_DescriptorCount{ 0 };
            uint32_t m_Marker{ 0 }; // TODO: Replace with allocator
        };

    public:
        DescriptorManager() = default;
        DescriptorManager(Device& device, const DescriptorManagerConfig& config);

    public:
        const DescriptorHeap& GetDescriptorHeap(DescriptorHeapType type) const { return m_DescriptorHeaps.at(type); }
        void Bind(Context& context, DescriptorHeapType type) const;

        RTVDescriptor AllocateRTV();
        DSVDescriptor AllocateDSV();
        SamplerDescriptor AllocateSampler();
        CBVDescriptor AllocateCBV();
        SRVDescriptor AllocateSRV();
        UAVDescriptor AllocateUAV();

    private:
        std::unordered_map<DescriptorHeapType, DescriptorHeap> m_DescriptorHeaps;
    };

} // namespace spieler::renderer