#pragma once

#include "benzin/graphics/api/common.hpp"

namespace benzin
{

    class ResourceLoader
    {
    public:
        BENZIN_NON_COPYABLE(ResourceLoader)
        BENZIN_NON_MOVEABLE(ResourceLoader)

    public:
        explicit ResourceLoader(const Device& device);

    public:
        std::shared_ptr<TextureResource> LoadTextureResourceFromDDSFile(const std::wstring_view& filepath, GraphicsCommandList& graphicsCommandList) const;

    private:
        const Device& m_Device;
    };

} // namespace benzin
