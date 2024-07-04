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

        static bool IsInitialized();

        static Bytes64 GetTotalDedicatedVram(uint32_t deviceId);
        static Bytes64 GetUsedDedicatedVram(uint32_t deviceId);

        static std::pair<Bytes64, Bytes64> GetCpuVisibleVram(ID3D12Device* d3d12Device);
    };

} // namespace benzin
