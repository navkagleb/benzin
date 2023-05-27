#pragma once

#include "benzin/graphics/api/resource.hpp"

namespace benzin
{

    class BufferResource final : public Resource
    {
    public:
        enum class Flags : uint8_t
        {
            None = 0,
            Dynamic = 1 << 0,
            ConstantBuffer = 1 << 1,
        };

        struct Config
        {
            uint32_t ElementSize{ 0 };
            uint32_t ElementCount{ 0 };
            Flags Flags{ Flags::None };
        };

    public:
        BufferResource(Device& device, const Config& config);

    public:
        const Config& GetConfig() const { return m_Config; }

    public:
        bool HasConstantBufferView(uint32_t index = 0) const;

        const Descriptor& GetConstantBufferView(uint32_t index = 0) const;

        uint32_t PushShaderResourceView();
        uint32_t PushConstantBufferView();

    private:
        Config m_Config;
    };

} // namespace benzin
