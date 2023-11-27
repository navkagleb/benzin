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

        static const GraphicsSettings& GetInstance()
        {
            static GraphicsSettings instance;
            return instance;
        }

    private:
        friend int ClientMain();

        static void Initialize(const GraphicsSettings& graphicsSettings)
        {
            const_cast<GraphicsSettings&>(GetInstance()) = graphicsSettings;
        }
    };

    inline const GraphicsSettings& g_GraphicsSettings = GraphicsSettings::GetInstance();

} // namespace benzin
