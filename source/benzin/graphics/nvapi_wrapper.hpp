#pragma once

namespace benzin
{

    struct PciIdentifiers;

    class NvApiWrapper
    {
    public:
        BenzinDefineNonConstructable(NvApiWrapper);

        static void Initialize();
        static void Shutdown();

        static uint64_t GetTotalDedicatedVramInBytes(const PciIdentifiers& id);
        static uint64_t GetUsedDedicatedVramInBytes(const PciIdentifiers& id);

        static std::pair<uint64_t, uint64_t> GetCpuVisibleVramInBytes(ID3D12Device* d3d12Device);
    };

} // namespace benzin
