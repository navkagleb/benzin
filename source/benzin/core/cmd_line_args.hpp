#pragma once

namespace benzin
{

    namespace CmdLineArgs
    {
        void Initialize(int argc, char** argv);

        auto GetWindowWidth() -> uint32_t;
        auto GetWindowHeight() -> uint32_t;

        auto IsPixCapturerEnabled() -> bool;
        auto IsAdlWrapperEnabled() -> bool;
        auto IsNvApiWrapperEnabled() -> bool;

        auto GetAdapterIndex() -> uint32_t;
        auto GetAdapterName() -> std::string_view;
        auto IsGpuUploadHeapsEnabled() -> bool;

        auto IsGpuValidationEnabled() -> bool;
        auto IsSynchronizedCommandQueueValidationEnabled() -> bool;

        auto IsShaderCacheIgnored() -> bool;
    };

}
