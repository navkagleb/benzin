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

    public:
        Shader() = default;

    private:
        Shader(ShaderType type, const std::wstring& filename, const std::string& entryPoint, const DefineContainer& defines);

    public:
        const void* GetData() const;
        uint32_t GetSize() const;

    protected:
        ComPtr<ID3DBlob> m_ByteCode;
    };

    template <typename Permutations>
    struct ShaderConfig
    {
        std::wstring Filename;
        std::string EntryPoint;
        ShaderPermutation<Permutations> Permutation;
    };

    class ShaderLibrary
    {
    public:
        template <typename Permutations>
        Shader& CreateShader(ShaderType Type, const ShaderConfig<Permutations>& config);

        template <typename Permutations>
        bool HasShader(ShaderType Type, const ShaderPermutation<Permutations>& permutation) const;

        template <typename Permutations>
        const Shader& GetShader(ShaderType type, const ShaderPermutation<Permutations>& permutation) const;

    private:
        std::unordered_map<ShaderType, std::unordered_map<uint64_t, Shader>> m_Shaders;
    };

} // namespace spieler:renderer

#include "shader.inl"