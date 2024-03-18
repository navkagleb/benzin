#pragma once

namespace benzin
{

    struct PciIdentifiers;

    class AdlWrapper
    {
    public:
        BenzinDefineNonConstructable(AdlWrapper);

        static void Initialize();
        static void Shutdown();

        static uint64_t GetUsedVramInBytes(uint32_t deviceId); // ???
        static uint64_t GetUsedDedicatedVramInBytes(uint32_t deviceId);
    };

} // namespace benzin
