#pragma once

#include "benzin/engine/model.hpp"
#include "benzin/graphics/api/common.hpp"

namespace benzin
{

    class PipelineState;

    class TextureLoader
    {
    public:
        BENZIN_NON_COPYABLE_IMPL(TextureLoader)
        BENZIN_NON_MOVEABLE_IMPL(TextureLoader)

    public:
        explicit TextureLoader(Device& device);

    public:
        TextureResource* LoadFromDDSFile(std::string_view fileName) const;
        TextureResource* LoadFromHDRFile(std::string_view fileName) const;
        TextureResource* LoadCubeMapFromHDRFile(std::string_view fileName, uint32_t size) const;

    private:
        void ConvertEquirectangularToCube(const TextureResource& equireactangularTexture, TextureResource& outCubeTexture) const;

    private:
        Device& m_Device;

        std::unique_ptr<PipelineState> m_EquirectangularToCubePipelineState;
    };

} // namespace benzin
