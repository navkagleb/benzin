#pragma once

#include "spieler/renderer/resource.hpp"
#include "spieler/renderer/resource_view.hpp"

namespace spieler::renderer
{

    namespace utils
    {

        template <std::integral T>
        constexpr inline static T Align(T value, T alignment)
        {
            return (value + alignment - 1) & ~(alignment - 1);
        }

    } // namespace utils

    class BufferResource : public Resource
    {
    public:
        friend class Device;

    public:
        enum class Flags : uint8_t
        {
            None = 0,
            Dynamic = 1 << 0,
            ConstantBuffer = 1 << 1
        };

        struct Config
        {
            uint32_t Alignment{ 0 };
            uint32_t ElementSize{ 0 };
            uint32_t ElementCount{ 0 };
            Flags Flags{ Flags::None };
        };

    public:
        BufferResource() = default;
        BufferResource(Device& device, const Config& config);

    public:
        const Config& GetConfig() const { return m_Config; }

    public:
        void Write(uint64_t offset, const void* data, uint64_t size);

    protected:
        Config m_Config;
    };

    struct Buffer
    {
        BufferResource Resource;
        ViewContainer Views{ Resource };
    };

} // namespace spieler::renderer
