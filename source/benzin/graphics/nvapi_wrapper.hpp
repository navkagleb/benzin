#pragma once

namespace benzin
{

    namespace NvApiWrapper
    {
        void Initialize();
        void Shutdown();

        Bytes64 GetTotalDedicatedVram(uint32_t deviceId);
        Bytes64 GetUsedDedicatedVram(uint32_t deviceId);

        std::pair<Bytes64, Bytes64> GetCpuVisibleVram(ID3D12Device* d3d12Device);
    };

}
