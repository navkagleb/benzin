#include "benzin/config/bootstrap.hpp"
#include "benzin/graphics/nvapi_wrapper.hpp"

#include <third_party/nvapi/include/nvapi.h>
#pragma comment(lib, "nvapi64.lib")

#include "benzin/core/asserter.hpp"
#include "benzin/core/logger.hpp"
#include "benzin/graphics/backend.hpp"

#define BenzinNvApiEnsure(nvCall) \
    const NvAPI_Status BenzinUniqueVariableName(nvStatus) = nvCall; \
    BenzinEnsure( \
        BenzinUniqueVariableName(nvStatus) == NVAPI_OK, \
        "NvAPIStatus - {} ({})", \
        magic_enum::enum_name(BenzinUniqueVariableName(nvStatus)), \
        magic_enum::enum_integer(BenzinUniqueVariableName(nvStatus)) \
    )

namespace benzin
{

    class NvApiState
    {
    public:
        NvApiState()
        {
            BenzinNvApiEnsure(NvAPI_Initialize());

            GatherAdapters();
        }

        ~NvApiState()
        {
            BenzinNvApiEnsure(NvAPI_Unload());
        }

        NvPhysicalGpuHandle GetPhysicalGpuHandle(const PciIdentifiers& id) const
        {
            BenzinAssert(m_PhysicalGpuHandles.contains(id));
            return m_PhysicalGpuHandles.at(id);
        }

        NV_GPU_MEMORY_INFO_EX GetGpuMemoryInfo(NvPhysicalGpuHandle gpuHandle)
        {
            NV_GPU_MEMORY_INFO_EX gpuMemoryInfo{};
            gpuMemoryInfo.version = NV_GPU_MEMORY_INFO_EX_VER_1;
            BenzinNvApiEnsure(NvAPI_GPU_GetMemoryInfoEx(gpuHandle, &gpuMemoryInfo));

            return gpuMemoryInfo;
        }

    private:
        void GatherAdapters()
        {
            NvPhysicalGpuHandle gpuHandles[NVAPI_MAX_PHYSICAL_GPUS];
            NvU32 gpuCount = 0;
            BenzinNvApiEnsure(NvAPI_EnumPhysicalGPUs(gpuHandles, &gpuCount));

            const std::span<const NvPhysicalGpuHandle> availablePhysicalGpuHandles{ gpuHandles, gpuCount };

            for (const auto [i, physicalGpuHandle] : availablePhysicalGpuHandles | std::views::enumerate)
            {
                NvAPI_ShortString gpuName;
                BenzinNvApiEnsure(NvAPI_GPU_GetFullName(physicalGpuHandle, gpuName));

                NvU32 deviceId = 0;
                NvU32 subSystemId = 0;
                NvU32 revisionId = 0;
                NvU32 extDeviceId = 0;
                BenzinNvApiEnsure(NvAPI_GPU_GetPCIIdentifiers(physicalGpuHandle, &deviceId, &subSystemId, &revisionId, &extDeviceId));

                const PciIdentifiers id
                {
                    .VendorId = 0x10DE, // Force set VendorId for NvAPI
                    .DeviceId = extDeviceId,
                    .SubSysId = subSystemId,
                    .RevisionId = revisionId,
                };

                BenzinTrace(
                    "Adapter {}. {}, VendorId: {}, DeviceId: {}, SubSysId: {}, RevisionId: {}",
                    i,
                    gpuName,
                    id.VendorId,
                    id.DeviceId,
                    id.SubSysId,
                    id.RevisionId
                );

                m_PhysicalGpuHandles[id] = physicalGpuHandle;
            }
        }

    private:
        std::unordered_map<PciIdentifiers, NvPhysicalGpuHandle> m_PhysicalGpuHandles;
    };

    static std::unique_ptr<NvApiState> g_NvApiState;

    //

    void NvApiWrapper::Initialize()
    {
        BenzinAssert(g_NvApiState.get() == nullptr);
        g_NvApiState = std::make_unique<NvApiState>();
    }

    void NvApiWrapper::Shutdown()
    {
        BenzinAssert(g_NvApiState.get() != nullptr);
        g_NvApiState.reset();
    }

    uint64_t NvApiWrapper::GetTotalDedicatedVramInBytes(const PciIdentifiers& id)
    {
        const NvPhysicalGpuHandle gpuHandle = g_NvApiState->GetPhysicalGpuHandle(id);
        const NV_GPU_MEMORY_INFO_EX gpuMemoryInfo = g_NvApiState->GetGpuMemoryInfo(gpuHandle);

        return gpuMemoryInfo.availableDedicatedVideoMemory;
    }

    uint64_t NvApiWrapper::GetUsedDedicatedVramInBytes(const PciIdentifiers& id)
    {
        const NvPhysicalGpuHandle gpuHandle = g_NvApiState->GetPhysicalGpuHandle(id);
        const NV_GPU_MEMORY_INFO_EX gpuMemoryInfo = g_NvApiState->GetGpuMemoryInfo(gpuHandle);

        return gpuMemoryInfo.availableDedicatedVideoMemory - gpuMemoryInfo.curAvailableDedicatedVideoMemory;
    }

    std::pair<uint64_t, uint64_t> NvApiWrapper::GetCpuVisibleVramInBytes(ID3D12Device* d3d12Device)
    {
        NvU64 totalSizeInBytes = 0;
        NvU64 freeSizeInBytes = 0;

        BenzinNvApiEnsure(NvAPI_D3D12_QueryCpuVisibleVidmem(d3d12Device, &totalSizeInBytes, &freeSizeInBytes));

        return std::make_pair(totalSizeInBytes, freeSizeInBytes);
    }

} // namespace benzin
