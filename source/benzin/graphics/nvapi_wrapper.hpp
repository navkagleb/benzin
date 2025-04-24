#pragma once

namespace benzin
{

    namespace NvApiWrapper
    {
        bool IsAvailable();

        void Initialize();
        void Shutdown();

        uint64_t GetTotalDedicatedVramInBytes(uint32_t deviceId);
        uint64_t GetUsedDedicatedVramInBytes(uint32_t deviceId);

        std::pair<uint64_t, uint64_t> GetCpuVisibleVramInBytes(ID3D12Device* d3d12Device);
    };

}
