#pragma once

namespace spieler
{

    class Shader
    {
    public:
        friend class ShaderLibrary;

    public:
        using DefineContainer = std::unordered_map<std::string_view, std::string_view>;

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
        static std::shared_ptr<Shader> Create(const Config& config);

    public:
        Shader() = default;
        Shader(const Config& config);
        ~Shader();

    private:
        void CompileFromFile(Type type, const std::wstring_view& filepath, const std::string_view& entryPoint, const DefineContainer& defines);

    public:
        const void* GetData() const;
        uint64_t GetSize() const;

    protected:
        ID3DBlob* m_DX12Shader{ nullptr };
    };

} // namespace spieler
