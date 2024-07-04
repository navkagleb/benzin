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

        static bool IsInitialized();

        static Bytes64 GetUsedVram(uint32_t deviceId); // ???
        static Bytes64 GetUsedDedicatedVram(uint32_t deviceId);
    };

} // namespace benzin
