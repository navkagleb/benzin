#pragma once

#include "spieler/graphics/common.hpp"

namespace spieler
{

    class ResourceLoader
    {
    public:
        SPIELER_NON_COPYABLE(ResourceLoader)
        SPIELER_NON_MOVEABLE(ResourceLoader)

    public:
        explicit ResourceLoader(const Device& device);

    public:
        std::shared_ptr<TextureResource> LoadTextureResourceFromDDSFile(const std::wstring_view& filepath, GraphicsCommandList& graphicsCommandList) const;

    private:
        const Device& m_Device;
    };

} // namespace spieler
