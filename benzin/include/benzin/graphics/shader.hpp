#pragma once

#include "benzin/graphics/common.hpp"

namespace benzin
{

    class Shader
    {
    public:
        BENZIN_NON_COPYABLE(Shader)
        BENZIN_NON_MOVEABLE(Shader)

    public:
        using DefineContainer = std::unordered_map<std::string_view, std::string>;

    public:
        enum class Type : uint8_t
        {
            Unknown,

            Vertex,
            Hull,
            Domain,
            Geometry,
            Pixel,
            Compute
        };

        struct Config
        {
            Type Type{ Type::Unknown };
            std::wstring_view Filepath;
            std::string_view EntryPoint;
            DefineContainer Defines;
        };

    public:
        Shader() = default;
        Shader(const Config& config);
        ~Shader();

    private:
        void CompileFromFile(Type type, const std::wstring_view& filepath, const std::string_view& entryPoint, const DefineContainer& defines);

    public:
        const void* GetData() const;
        uint64_t GetSize() const;

    private:
        ID3DBlob* m_D3D12Shader{ nullptr };
    };

} // namespace benzin
