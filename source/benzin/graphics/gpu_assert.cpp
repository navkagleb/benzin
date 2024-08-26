#include "benzin/config/bootstrap.hpp"
#include "benzin/graphics/gpu_assert.hpp"

#include "benzin/graphics/device.hpp"
#include "benzin/graphics/buffer.hpp"

namespace benzin
{

    GpuAssert::GpuAssert(Device& device)
        : m_Device{ device }
    {
        const BufferCreation creation
        {
            .DebugName = "GpuAssert",
            .Type = BufferType::Byte,
            .ElementSize = sizeof(std::byte),
            .ElementCount = 1,
            .IsUnorderedAccessAllowed = true,
        };

        Buffer buffer{ device, creation };

        m_CrashUavDescriptor = buffer.CreateDetachedUav();
    }

    GpuAssert::~GpuAssert()
    {
        m_Device.GetDescriptorManager().FreeDescriptor(m_CrashUavDescriptor);
    }

}
