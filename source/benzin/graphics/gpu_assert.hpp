#pragma once

#include "benzin/graphics/descriptor_manager.hpp"

namespace benzin
{

    class Device;

    class GpuAssert
    {
    public:
        explicit GpuAssert(Device& device);
        ~GpuAssert();

    private:
        Device& m_Device;

        Descriptor m_CrashUavDescriptor;
    };

}
