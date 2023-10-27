#pragma once

#include "benzin/engine/model.hpp"
#include "benzin/graphics/common.hpp"

namespace benzin
{

    class PipelineState;

    class TextureLoader
    {
    public:
        BenzinDefineNonCopyable(TextureLoader);
        BenzinDefineNonMoveable(TextureLoader);

    public:
        explicit TextureLoader(Device& device);

    public:
        Texture* LoadFromDDSFile(std::string_view fileName) const;
        Texture* LoadFromHDRFile(std::string_view fileName) const;
        Texture* LoadCubeMapFromHDRFile(std::string_view fileName, uint32_t size) const;

    private:
        void ToEquirectangularToCube(const Texture& equireactangularTexture, Texture& outCubeTexture) const;

    private:
        Device& m_Device;

        std::unique_ptr<PipelineState> m_EquirectangularToCubePipelineState;
    };

} // namespace benzin
