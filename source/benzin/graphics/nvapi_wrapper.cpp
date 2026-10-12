#include <benzin/config/bootstrap.hpp>
#include <benzin/graphics/nvapi_wrapper.hpp>

#include <benzin/core/cmd_line_args.hpp>

#include <nvapi.h>

#define BenzinNvApiEnsure(nvCall) \
    const NvAPI_Status BenzinUniqueVariableName(nvStatus) = nvCall; \
    BenzinEnsure( \
        BenzinUniqueVariableName(nvStatus) == NVAPI_OK, \
        "NvAPIStatus - {} ({})", \
        magic_enum::enum_name(BenzinUniqueVariableName(nvStatus)), \
        magic_enum::enum_integer(BenzinUniqueVariableName(nvStatus)))

namespace benzin
{

    struct NvApiState
    {
        NvPhysicalGpuHandle m_GpuHandle = {}; // Let's assume that there is no more than one Nvidia GPU in the system
        bool m_IsInitialized = false;

        void Initialize();
        void Shutdown();

        NV_GPU_MEMORY_INFO_EX GetGpuMemoryInfo();
    };

    void NvApiState::Initialize()
    {
        m_IsInitialized = NvAPI_Initialize() == NVAPI_OK;

        if (!m_IsInitialized)
            return;

        NvU32 driverVersion = 0;
        NvAPI_ShortString buildBranch{};
        NvAPI_SYS_GetDriverAndBranchVersion(&driverVersion, buildBranch);
        BenzinTrace("[NvAPI DriverVersion: {}.{}, (BuildBranch: {})", driverVersion / 100, driverVersion % 100, buildBranch);

        NvPhysicalGpuHandle gpuHandles[NVAPI_MAX_PHYSICAL_GPUS];
        NvU32 gpuCount = 0;
        BenzinNvApiEnsure(NvAPI_EnumPhysicalGPUs(gpuHandles, &gpuCount));

        BenzinAssert(gpuCount == 1);
        m_GpuHandle = gpuHandles[0];

        NvAPI_ShortString gpuName;
        BenzinNvApiEnsure(NvAPI_GPU_GetFullName(m_GpuHandle, gpuName));

        NvU32 deviceId = 0;
        NvU32 subSystemId = 0;
        NvU32 revisionId = 0;
        NvU32 extDeviceId = 0;
        BenzinNvApiEnsure(NvAPI_GPU_GetPCIIdentifiers(m_GpuHandle, &deviceId, &subSystemId, &revisionId, &extDeviceId));

        BenzinTrace(
            "[NvAPI] Adapter = {}, VendorId: {}, DeviceId: {}, SubSysId: {}, RevisionId: {}",
            gpuName,
            0x10DE, // Force set VendorId for NvAPI,
            extDeviceId,
            subSystemId,
            revisionId);
    }

    void NvApiState::Shutdown()
    {
        if (m_IsInitialized)
        {
            BenzinNvApiEnsure(NvAPI_Unload());
            m_IsInitialized = false;
        }
    }

    NV_GPU_MEMORY_INFO_EX NvApiState::GetGpuMemoryInfo()
    {
        BenzinAssert(m_IsInitialized);

        NV_GPU_MEMORY_INFO_EX gpuMemoryInfo = {};
        gpuMemoryInfo.version = NV_GPU_MEMORY_INFO_EX_VER_1;
        BenzinNvApiEnsure(NvAPI_GPU_GetMemoryInfoEx(m_GpuHandle, &gpuMemoryInfo));

        return gpuMemoryInfo;
    }

    static NvApiState g_NvApiState;

    //

    bool NvApiWrapper::IsAvailable()
    {
        return g_NvApiState.m_IsInitialized;
    }

    void NvApiWrapper::Initialize()
    {
        if (CmdLineArgs::IsPixCapturerEnabled())
        {
            // PIX for windows says: PIX has detected that the application was using NVAPI when this capture was taken
            // This may result in PIX crashing during analysis and/or PIX showing misleading data
            return;
        }

        if (CmdLineArgs::IsNvApiWrapperEnabled())
        {
            g_NvApiState.Initialize();
        }
    }

    void NvApiWrapper::Shutdown()
    {
        g_NvApiState.Shutdown();
    }

    uint32_t NvApiWrapper::GetGpuCoreCount()
    {
        NvU32 gpuCoreCount = 0;
        BenzinNvApiEnsure(NvAPI_GPU_GetGpuCoreCount(g_NvApiState.m_GpuHandle, &gpuCoreCount));

        return gpuCoreCount;
    }

    uint64_t NvApiWrapper::GetTotalDedicatedVramInBytes()
    {
        const NV_GPU_MEMORY_INFO_EX gpuMemoryInfo = g_NvApiState.GetGpuMemoryInfo();
        return gpuMemoryInfo.availableDedicatedVideoMemory;
    }

    uint64_t NvApiWrapper::GetUsedDedicatedVramInBytes()
    {
        const NV_GPU_MEMORY_INFO_EX gpuMemoryInfo = g_NvApiState.GetGpuMemoryInfo();
        return gpuMemoryInfo.availableDedicatedVideoMemory - gpuMemoryInfo.curAvailableDedicatedVideoMemory;
    }

}
