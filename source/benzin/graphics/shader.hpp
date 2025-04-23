#pragma once

namespace benzin
{

    using ShaderBytecode = std::span<const std::byte>;

    enum class ShaderType : uint8_t
    {
        Vertex,
        Pixel,
        Compute,

        Library,

        Amplification,
        Mesh,
    };
    BenzinEnableUnaryPlusForEnum(ShaderType);

    class ShaderInfo
    {
    public:
        ShaderInfo() = default;
        ShaderInfo(ShaderType type, std::string_view fileName, std::string_view entryPoint, std::vector<std::string_view>&& defines);

        auto GetType() const { return m_Type; }
        auto GetFileName() const { return m_FileName; }
        auto GetEntryPoint() const { return m_EntryPoint; }
        auto GetDefines() const { return std::span<const std::string_view>{ m_Defines }; }

        auto GetHash() const { return m_Hash; }

        bool IsValid() const;

    private:
        ShaderType m_Type = g_BadEnum<ShaderType>;
        std::string_view m_FileName;
        std::string_view m_EntryPoint;
        std::vector<std::string_view> m_Defines;

        uint64_t m_Hash = g_Bad64;
    };

}
