#pragma once

#include <third_party/magic_enum/magic_enum.hpp>

namespace spieler::renderer
{

    enum class ShaderType : uint8_t
    {
        None,
        Vertex,
        Pixel,
        Geometry
    };

    using DefineContainer = std::unordered_map<std::string_view, std::string>;

    template <typename Permutations>
    class ShaderPermutation
    {
    public:
        friend class ShaderLibrary;

    public:
        template <Permutations Permutation, typename T>
        void Set(T value);

        const DefineContainer& GetDefines() const { return m_Defines; }

    private:
        uint64_t GetHash() const;

    private:
        DefineContainer m_Defines;
    };

    class Shader
    {
    public:
        friend class ShaderLibrary;

    private:
        struct Config
        {
            ShaderType Type{ ShaderType::None };
            std::wstring Filename;
            std::string EntryPoint;
            DefineContainer Defines;
        };

    public:
        Shader() = default;

    private:
        Shader(const Config& config);

    public:
        const void* GetData() const;
        uint32_t GetSize() const;

    protected:
        ComPtr<ID3DBlob> m_ByteCode;
    };

    class ShaderLibrary
    {
    public:
        template <ShaderType Type, typename Permutations>
        Shader& CreateShader(const std::wstring& filename, const std::string& entryPoint, const ShaderPermutation<Permutations>& permutations);

        template <ShaderType Type, typename Permutations>
        bool HasShader(const ShaderPermutation<Permutations>& permutation) const;

        template <ShaderType Type, typename Permutations>
        const Shader& GetShader(const ShaderPermutation<Permutations>& permutation) const;

    private:
        std::unordered_map<ShaderType, std::unordered_map<uint64_t, Shader>> m_Shaders;
    };

} // namespace spieler:renderer

#include "shader.inl"