#pragma once

namespace benzin
{

    namespace CmdLineArgs
    {
        void Initialize(int argc, char** argv);

        auto GetRawLogOptionFlags() -> uint32_t;

        auto GetWindowWidth() -> uint32_t;
        auto GetWindowHeight() -> uint32_t;
        auto IsWindowResizable() -> bool;

        auto IsPixCapturerEnabled() -> bool;
        auto IsAdlWrapperEnabled() -> bool;
        auto IsNvApiWrapperEnabled() -> bool;

        auto GetAdapterIndex() -> uint32_t;
        auto GetAdapterName() -> std::string_view;
        auto GetFrameInFlightCount() -> uint32_t;
        auto GetBackBufferFormat() -> GraphicsFormat;
        auto IsGpuUploadHeapsEnabled() -> bool;

        auto IsGpuValidationEnabled() -> bool;
        auto IsSynchronizedCommandQueueValidationEnabled() -> bool;

        auto IsShaderCacheIgnored() -> bool;
    };

}
