#pragma once

namespace benzin
{

    namespace NvApiWrapper
    {
        bool IsAvailable();

        void Initialize();
        void Shutdown();

        uint32_t GetGpuCoreCount();

        uint64_t GetTotalDedicatedVramInBytes();
        uint64_t GetUsedDedicatedVramInBytes();
    };

}
