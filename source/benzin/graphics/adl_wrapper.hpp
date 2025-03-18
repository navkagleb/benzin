#pragma once

namespace benzin
{

    namespace AdlWrapper
    {
        bool IsAvailable();
        void Initialize();
        void Shutdown();

        Bytes64 GetUsedVram(uint32_t deviceId); // ???
        Bytes64 GetUsedDedicatedVram(uint32_t deviceId);
    };

}
