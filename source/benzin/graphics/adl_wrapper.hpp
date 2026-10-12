#pragma once

namespace benzin
{

    namespace AdlWrapper
    {
        bool IsAvailable();
        void Initialize();
        void Shutdown();

        uint32_t GetGpuCoreCount();

        uint64_t GetUsedVramInBytes();
        uint64_t GetUsedDedicatedVramInBytes();
    };

}
