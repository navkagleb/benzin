#pragma once

#include "benzin/graphics/format.hpp"

namespace benzin
{

    struct DebugLayerParams
    {
        bool IsGPUBasedValidationEnabled = true;
        bool IsSynchronizedCommandQueueValidationEnabled = true;
    };

    struct GraphicsSettings
    {
        uint32_t MainAdapterIndex = 0;
        uint32_t FrameInFlightCount = 3;
        GraphicsFormat BackBufferFormat = GraphicsFormat::RGBA8Unorm;
        DebugLayerParams DebugLayerParams;
    };

    using GraphicsSettingsInstance = SingletonInstanceWrapper<GraphicsSettings>;

} // namespace benzin
