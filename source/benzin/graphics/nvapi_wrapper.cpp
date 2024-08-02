#include "benzin/config/bootstrap.hpp"
#include "benzin/graphics/nvapi_wrapper.hpp"

#include <third_party/nvapi/include/nvapi.h>
#pragma comment(lib, "nvapi64.lib")

#include "benzin/core/asserter.hpp"
#include "benzin/core/command_line_args.hpp"
#include "benzin/core/logger.hpp"

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

        NvPhysicalGpuHandle GetPhysicalGpuHandle(uint32_t deviceId) const
        {
            BenzinAssert(m_PhysicalGpuHandles.contains(deviceId));
            return m_PhysicalGpuHandles.at(deviceId);
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

                BenzinTrace(
                    "NvApi Adapter {}. {}, VendorId: {}, DeviceId: {}, SubSysId: {}, RevisionId: {}",
                    i,
                    gpuName,
                    0x10DE, // Force set VendorId for NvAPI,
                    extDeviceId,
                    subSystemId,
                    revisionId
                );

                m_PhysicalGpuHandles[extDeviceId] = physicalGpuHandle;
            }
        }

    private:
        std::unordered_map<uint32_t, NvPhysicalGpuHandle> m_PhysicalGpuHandles;
    };

    static std::unique_ptr<NvApiState> g_NvApiState;

    //

    void NvApiWrapper::Initialize()
    {
        if (CommandLineArgs::g_IsNvApiWrapperEnabled)
        {
            MakeUniquePtr(g_NvApiState);
        }
    }

    void NvApiWrapper::Shutdown()
    {
        g_NvApiState.reset();
    }

    bool NvApiWrapper::IsInitialized()
    {
        return g_NvApiState.get();
    }

    Bytes64 NvApiWrapper::GetTotalDedicatedVram(uint32_t deviceId)
    {
        const NvPhysicalGpuHandle gpuHandle = g_NvApiState->GetPhysicalGpuHandle(deviceId);
        const NV_GPU_MEMORY_INFO_EX gpuMemoryInfo = g_NvApiState->GetGpuMemoryInfo(gpuHandle);

        return gpuMemoryInfo.availableDedicatedVideoMemory;
    }

    Bytes64 NvApiWrapper::GetUsedDedicatedVram(uint32_t deviceId)
    {
        const NvPhysicalGpuHandle gpuHandle = g_NvApiState->GetPhysicalGpuHandle(deviceId);
        const NV_GPU_MEMORY_INFO_EX gpuMemoryInfo = g_NvApiState->GetGpuMemoryInfo(gpuHandle);

        return gpuMemoryInfo.availableDedicatedVideoMemory - gpuMemoryInfo.curAvailableDedicatedVideoMemory;
    }

    std::pair<Bytes64, Bytes64> NvApiWrapper::GetCpuVisibleVram(ID3D12Device* d3d12Device)
    {
        NvU64 totalSize = 0;
        NvU64 freeSize = 0;

        BenzinNvApiEnsure(NvAPI_D3D12_QueryCpuVisibleVidmem(d3d12Device, &totalSize, &freeSize));

        return std::make_pair(totalSize, freeSize);
    }

} // namespace benzin
