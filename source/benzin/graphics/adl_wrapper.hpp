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

        static uint64_t GetUsedVramInBytes(const PciIdentifiers& id); // ???
        static uint64_t GetUsedDedicatedVramInBytes(const PciIdentifiers& id);
    };

} // namespace benzin
