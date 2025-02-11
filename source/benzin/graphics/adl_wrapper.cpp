#include "benzin/config/bootstrap.hpp"
#include "benzin/graphics/adl_wrapper.hpp"

#include <adl_sdk.h>

#include "benzin/core/command_line_args.hpp"
#include "benzin/core/logger.hpp"

static constexpr std::string_view AdlReturnCodeToString(int adlReturnCode)
{
    switch (adlReturnCode)
    {
        case ADL_OK_WAIT: return BenzinStringify(ADL_OK_WAIT);
        case ADL_OK_RESTART: return BenzinStringify(ADL_OK_RESTART);
        case ADL_OK_MODE_CHANGE: return BenzinStringify(ADL_OK_MODE_CHANGE);
        case ADL_OK_WARNING: return BenzinStringify(ADL_OK_WARNING);
        case ADL_OK: return BenzinStringify(ADL_OK);
        case ADL_ERR: return BenzinStringify(ADL_ERR);
        case ADL_ERR_NOT_INIT: return BenzinStringify(ADL_ERR_NOT_INIT);
        case ADL_ERR_INVALID_PARAM: return BenzinStringify(ADL_ERR_INVALID_PARAM);
        case ADL_ERR_INVALID_PARAM_SIZE: return BenzinStringify(ADL_ERR_INVALID_PARAM_SIZE);
        case ADL_ERR_INVALID_ADL_IDX: return BenzinStringify(ADL_ERR_INVALID_ADL_IDX);
        case ADL_ERR_INVALID_CONTROLLER_IDX: return BenzinStringify(ADL_ERR_INVALID_CONTROLLER_IDX);
        case ADL_ERR_INVALID_DIPLAY_IDX: return BenzinStringify(ADL_ERR_INVALID_DIPLAY_IDX);
        case ADL_ERR_NOT_SUPPORTED: return BenzinStringify(ADL_ERR_NOT_SUPPORTED);
        case ADL_ERR_NULL_POINTER: return BenzinStringify(ADL_ERR_NULL_POINTER);
        case ADL_ERR_DISABLED_ADAPTER: return BenzinStringify(ADL_ERR_DISABLED_ADAPTER);
        case ADL_ERR_INVALID_CALLBACK: return BenzinStringify(ADL_ERR_INVALID_CALLBACK);
        case ADL_ERR_RESOURCE_CONFLICT: return BenzinStringify(ADL_ERR_RESOURCE_CONFLICT);
        case ADL_ERR_SET_INCOMPLETE: return BenzinStringify(ADL_ERR_SET_INCOMPLETE);
        case ADL_ERR_CALL_TO_INCOMPATIABLE_DRIVER: return BenzinStringify(ADL_ERR_CALL_TO_INCOMPATIABLE_DRIVER);
        case ADL_ERR_NO_ADMINISTRATOR_PRIVILEGES: return BenzinStringify(ADL_ERR_NO_ADMINISTRATOR_PRIVILEGES);
        case ADL_ERR_FEATURESYNC_NOT_STARTED: return BenzinStringify(ADL_ERR_FEATURESYNC_NOT_STARTED);
        case ADL_ERR_INVALID_POWER_STATE: return BenzinStringify(ADL_ERR_INVALID_POWER_STATE);
    }

    return std::string_view{};
}

#define BenzinAdlEnsure(adlCall) \
    const int BenzinUniqueVariableName(adlReturnCode) = adlCall; \
    BenzinEnsure( \
        BenzinUniqueVariableName(adlReturnCode) == ADL_OK, \
        "AdlReturnedCode - {} ({})", \
        AdlReturnCodeToString(BenzinUniqueVariableName(adlReturnCode)), \
        BenzinUniqueVariableName(adlReturnCode) \
    )

namespace benzin
{

    using ADL2_MAIN_CONTROL_CREATE = int (*)(ADL_MAIN_MALLOC_CALLBACK, int, ADL_CONTEXT_HANDLE*);
    using ADL2_MAIN_CONTROL_DESTROY =  int (*)(ADL_CONTEXT_HANDLE);

    using ADL2_ADAPTER_ADAPTERINFOX4_GET = int (*)(ADL_CONTEXT_HANDLE context, int iAdapterIndex, int* numAdapters, AdapterInfoX2** lppAdapterInfoX2);
    using ADL2_ADAPTER_DEDICATEDVRAMUSAGE_GET = int (*)(ADL_CONTEXT_HANDLE context, int iAdapterIndex, int* iVRAMUsageInMB);
    using ADL2_ADAPTER_ID_GET = int (*)(ADL_CONTEXT_HANDLE, int, int*);
    using ADL2_ADAPTER_VRAMEUSAGE_GET = int (*)(ADL_CONTEXT_HANDLE context, int iAdapterIndex, int* iVRAMUsageInMB);

    // The function names must be the same as in the ADL library
    ADL2_MAIN_CONTROL_CREATE ADL2_Main_Control_Create = nullptr;
    ADL2_MAIN_CONTROL_DESTROY ADL2_Main_Control_Destroy = nullptr;

    ADL2_ADAPTER_ADAPTERINFOX4_GET ADL2_Adapter_AdapterInfoX4_Get = nullptr;
    ADL2_ADAPTER_DEDICATEDVRAMUSAGE_GET ADL2_Adapter_DedicatedVRAMUsage_Get = nullptr;
    ADL2_ADAPTER_ID_GET ADL2_Adapter_ID_Get = nullptr;
    ADL2_ADAPTER_VRAMEUSAGE_GET ADL2_Adapter_VRAMUsage_Get = nullptr;

    void* __stdcall ADL_Main_Memory_Alloc(int size)
    {
        auto* buffer = new std::byte[size];
        return (void*)buffer;
    }

    void __stdcall ADL_Main_Memory_Free(void* buffer)
    {
        if (buffer)
        {
            delete (std::byte*)buffer;
            buffer = nullptr;
        }
    }

    static void ParseUniqueDeviceIdString(std::string_view udid, uint32_t& outVendorId, uint32_t& outDeviceId, uint32_t& outSubSysId, uint32_t& outRevisionId)
    {
        static constexpr std::string_view vendorKey = "VEN_";
        static constexpr std::string_view deviceKey = "DEV_";
        static constexpr std::string_view subSysKey = "SUBSYS_";
        static constexpr std::string_view revisionKey = "REV_";

        static const auto GetValue = [](std::string_view udid, std::string_view key)
        {
            // Delimiter between keys is '&' or '_'
            // Example: PCI_VEN_1002&DEV_73BF&SUBSYS_23291458&REV_C3_6&2201046F&0&00000008A
            // vendor - 1002
            // device - 73BF
            // subSys - 23291458
            // revision - C3

            const size_t keyBeginIndex = udid.find(key);
            BenzinAssert(keyBeginIndex != std::string_view::npos);

            const size_t keyEndIndex = keyBeginIndex + key.size();
            const size_t nextKeyKeyIndex = udid.find_first_of("&_", keyEndIndex);
            const size_t keyValueLength = nextKeyKeyIndex - keyEndIndex;

            return udid.substr(keyEndIndex, keyValueLength);
        };

        static const auto ConvertFrom16to10Base = [](std::string_view valueString)
        {
            uint32_t value = g_InvalidUnsigned<uint32_t>;
            const auto result = std::from_chars(valueString.data(), valueString.data() + valueString.size(), value, 16);
            BenzinAssert(result.ec == std::errc{} && IsValidUnsigned(value));

            return value;
        };

        const auto vendorString = GetValue(udid, vendorKey);
        const auto deviceString = GetValue(udid, deviceKey);
        const auto subSysString = GetValue(udid, subSysKey);
        const auto revisionString = GetValue(udid, revisionKey);

        outVendorId = ConvertFrom16to10Base(vendorString);
        outDeviceId = ConvertFrom16to10Base(deviceString);
        outSubSysId = ConvertFrom16to10Base(subSysString);
        outRevisionId = ConvertFrom16to10Base(revisionString);
    }

    class AdlState
    {
    public:
        AdlState()
        {
            m_DllHandle = ::LoadLibrary("atiadlxx.dll");
            BenzinAssert(m_DllHandle != nullptr);

            ADL2_Main_Control_Create = (decltype(ADL2_Main_Control_Create))::GetProcAddress(m_DllHandle, BenzinStringify(ADL2_Main_Control_Create));
            ADL2_Main_Control_Destroy = (decltype(ADL2_Main_Control_Destroy))::GetProcAddress(m_DllHandle, BenzinStringify(ADL2_Main_Control_Destroy));

            ADL2_Adapter_AdapterInfoX4_Get = (decltype(ADL2_Adapter_AdapterInfoX4_Get))::GetProcAddress(m_DllHandle, BenzinStringify(ADL2_Adapter_AdapterInfoX4_Get));
            ADL2_Adapter_DedicatedVRAMUsage_Get = (decltype(ADL2_Adapter_DedicatedVRAMUsage_Get))::GetProcAddress(m_DllHandle, BenzinStringify(ADL2_Adapter_DedicatedVRAMUsage_Get));
            ADL2_Adapter_ID_Get = (decltype(ADL2_Adapter_ID_Get))::GetProcAddress(m_DllHandle, BenzinStringify(ADL2_Adapter_ID_Get));
            ADL2_Adapter_VRAMUsage_Get = (decltype(ADL2_Adapter_VRAMUsage_Get))::GetProcAddress(m_DllHandle, BenzinStringify(ADL2_Adapter_VRAMUsage_Get));

            BenzinAssert(ADL2_Main_Control_Create != nullptr);
            BenzinAssert(ADL2_Main_Control_Destroy != nullptr);

            BenzinAssert(ADL2_Adapter_AdapterInfoX4_Get != nullptr);
            BenzinAssert(ADL2_Adapter_DedicatedVRAMUsage_Get != nullptr);
            BenzinAssert(ADL2_Adapter_ID_Get != nullptr);
            BenzinAssert(ADL2_Adapter_VRAMUsage_Get != nullptr);

            BenzinAdlEnsure(ADL2_Main_Control_Create(ADL_Main_Memory_Alloc, 1, &m_Context));

            GatherAdapters();
        }

        ~AdlState()
        {
            BenzinAdlEnsure(ADL2_Main_Control_Destroy(m_Context));

            ::FreeLibrary(m_DllHandle);
        }

        int GetAdapterIndex(uint32_t deviceId) const
        {
            BenzinAssert(m_AdlAdapterIndices.contains(deviceId));
            return m_AdlAdapterIndices.at(deviceId);
        }

        Bytes64 GetUsedVram(int adlAdapterIndex)
        {
            int vramUsageInMb = 0;
            BenzinAdlEnsure(ADL2_Adapter_VRAMUsage_Get(m_Context, adlAdapterIndex, &vramUsageInMb));

            return vramUsageInMb * 1_mb;
        }

        Bytes64 GetUsedDedicatedVram(int adlAdapterIndex)
        {
            int vramUsageInMb = 0;
            BenzinAdlEnsure(ADL2_Adapter_DedicatedVRAMUsage_Get(m_Context, adlAdapterIndex, &vramUsageInMb));

            return vramUsageInMb * 1_mb;
        }

    private:
        void GatherAdapters()
        {
            const int adapterIndex = -1; // Enum all adapters
            int adapterCount = 0;
            AdapterInfoX2* adapterInfoData = nullptr;
            BenzinAdlEnsure(ADL2_Adapter_AdapterInfoX4_Get(m_Context, adapterIndex, &adapterCount, &adapterInfoData));

            const std::span<const AdapterInfoX2> adapterInfos{ adapterInfoData, (size_t)adapterCount };

            std::set<int> existedBusIndices;
            for (const auto& [i, adapterInfo] : adapterInfos | std::views::enumerate)
            {
                const int adlAdapterIndex = adapterInfo.iAdapterIndex;

                if (!IsAmdAdapter(adlAdapterIndex) || existedBusIndices.contains(adapterInfo.iBusNumber))
                {
                    continue;
                }

                existedBusIndices.insert(adapterInfo.iBusNumber);

                uint32_t vendorId;
                uint32_t deviceId;
                uint32_t subSysId;
                uint32_t revisionId;
                ParseUniqueDeviceIdString(adapterInfo.strUDID, vendorId, deviceId, subSysId, revisionId);

                BenzinTrace(
                    "Adl Adapter {}. {}, VendorId: {}, DeviceId: {}, SubSysId: {}, RevisionId: {}",
                    i,
                    adapterInfo.strAdapterName,
                    vendorId,
                    deviceId,
                    subSysId,
                    revisionId
                );

                m_AdlAdapterIndices[deviceId] = adlAdapterIndex;
            }

            ADL_Main_Memory_Free(adapterInfoData);
        }

        bool IsAmdAdapter(int adlAdapterIndex)
        {
            int uniqueAdapterId = 0;
            if (ADL2_Adapter_ID_Get(m_Context, adlAdapterIndex, &uniqueAdapterId) != ADL_OK)
            {
                // Fail when AMD adapter disabled via Windows DeviceManager
                return false;
            }

            return uniqueAdapterId != 0; // Zero means: The adapter is not AMD
        }

    private:
        HINSTANCE m_DllHandle = nullptr;
        ADL_CONTEXT_HANDLE m_Context = nullptr;

        std::unordered_map<uint32_t, int> m_AdlAdapterIndices;
    };

    static std::unique_ptr<AdlState> g_AdlState;

    //

    void AdlWrapper::Initialize()
    {
        if (CommandLineArgs::GetBool("IsAdlWrapperEnabled"))
        {
            MakeUniquePtr(g_AdlState);
        }
    }

    void AdlWrapper::Shutdown()
    {
        g_AdlState.reset();
    }

    bool AdlWrapper::IsInitialized()
    {
        return g_AdlState.get();
    }

    Bytes64 AdlWrapper::GetUsedVram(uint32_t deviceId)
    {
        const uint32_t internalAdapterIndex = g_AdlState->GetAdapterIndex(deviceId);
        return g_AdlState->GetUsedVram(internalAdapterIndex);
    }

    Bytes64 AdlWrapper::GetUsedDedicatedVram(uint32_t deviceId)
    {
        const uint32_t internalAdapterIndex = g_AdlState->GetAdapterIndex(deviceId);
        return g_AdlState->GetUsedDedicatedVram(internalAdapterIndex);
    }

} // namespace benzin
