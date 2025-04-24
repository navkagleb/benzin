#pragma once

namespace benzin
{

    namespace AdlWrapper
    {
        bool IsAvailable();
        void Initialize();
        void Shutdown();

        uint64_t GetUsedVramInBytes(uint32_t deviceId); // ???
        uint64_t GetUsedDedicatedVramInBytes(uint32_t deviceId);
    };

}
