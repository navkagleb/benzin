#include "benzin/config/bootstrap.hpp"
#include "benzin/graphics/nvapi_wrapper.hpp"

#include <nvapi.h>

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
        friend bool NvApiWrapper::IsAvailable();

        void Initialize()
        {
            m_IsInitialized = NvAPI_Initialize() == NVAPI_OK;
            
            if (!m_IsInitialized)
            {
                return;
            }

            GatherAdapters();
        }

        void Shutdown()
        {
            if (m_IsInitialized)
            {
                BenzinNvApiEnsure(NvAPI_Unload());
                m_IsInitialized = false;
            }
        }

        NvPhysicalGpuHandle GetPhysicalGpuHandle(uint32_t deviceId) const
        {
            BenzinAssert(m_IsInitialized);
            BenzinAssert(m_PhysicalGpuHandles.contains(deviceId));

            return m_PhysicalGpuHandles.at(deviceId);
        }

        NV_GPU_MEMORY_INFO_EX GetGpuMemoryInfo(NvPhysicalGpuHandle gpuHandle)
        {
            BenzinAssert(m_IsInitialized);

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
        bool m_IsInitialized = false;
    };

    static NvApiState g_NvApiState;

    //

    bool NvApiWrapper::IsAvailable()
    {
        return g_NvApiState.m_IsInitialized;
    }

    void NvApiWrapper::Initialize()
    {
        if (CommandLineArgs::GetBool("IsPixCapturerEnabled"))
        {
            // PIX for windows says: PIX has detected that the application was using NVAPI when this capture was taken
            // This may result in PIX crashing during analysis and/or PIX showing misleading data
            return;
        }

        if (CommandLineArgs::GetBool("IsNvApiWrapperEnabled"))
        {
            g_NvApiState.Initialize();
        }
    }

    void NvApiWrapper::Shutdown()
    {
        g_NvApiState.Shutdown();
    }

    Bytes64 NvApiWrapper::GetTotalDedicatedVram(uint32_t deviceId)
    {
        const NvPhysicalGpuHandle gpuHandle = g_NvApiState.GetPhysicalGpuHandle(deviceId);
        const NV_GPU_MEMORY_INFO_EX gpuMemoryInfo = g_NvApiState.GetGpuMemoryInfo(gpuHandle);

        return gpuMemoryInfo.availableDedicatedVideoMemory;
    }

    Bytes64 NvApiWrapper::GetUsedDedicatedVram(uint32_t deviceId)
    {
        const NvPhysicalGpuHandle gpuHandle = g_NvApiState.GetPhysicalGpuHandle(deviceId);
        const NV_GPU_MEMORY_INFO_EX gpuMemoryInfo = g_NvApiState.GetGpuMemoryInfo(gpuHandle);

        return gpuMemoryInfo.availableDedicatedVideoMemory - gpuMemoryInfo.curAvailableDedicatedVideoMemory;
    }

    std::pair<Bytes64, Bytes64> NvApiWrapper::GetCpuVisibleVram(ID3D12Device* d3d12Device)
    {
        NvU64 totalSize = 0;
        NvU64 freeSize = 0;

        BenzinNvApiEnsure(NvAPI_D3D12_QueryCpuVisibleVidmem(d3d12Device, &totalSize, &freeSize));

        return std::make_pair(totalSize, freeSize);
    }

}
