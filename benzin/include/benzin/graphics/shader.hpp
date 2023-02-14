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
        struct Define
        {
            std::string_view Name;
            std::string Value;
        };

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

        struct Register
        {
            uint32_t Index{ 0 };
            uint32_t Space{ 0 };
        };

        struct Config
        {
            Type Type{ Type::Unknown };
            std::wstring_view Filepath;
            std::string_view EntryPoint;
            std::vector<Define> Defines;
        };

    public:
        Shader() = default;
        Shader(const Config& config);
        ~Shader();

    private:
        void CompileFromFile(Type type, const std::wstring_view& filepath, const std::string_view& entryPoint, const std::vector<Define>& defines);

    public:
        const void* GetData() const;
        uint64_t GetSize() const;

    private:
        ID3DBlob* m_D3D12Shader{ nullptr };
    };

} // namespace benzin
