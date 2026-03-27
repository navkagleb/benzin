#pragma once

namespace benzin
{

    class ShaderInfo;

    struct ShaderPaths
    {
        std::filesystem::path SourceFilePath;
        std::filesystem::path DxilFilePath;
        std::filesystem::path PdbFilePath;

        explicit ShaderPaths(const ShaderInfo& shader);
    };

    struct ShaderArgs
    {
        std::wstring_view Target;
        std::wstring EntryPoint;
        std::vector<std::wstring> Defines;

        explicit ShaderArgs(const ShaderInfo& shader);
    };

    struct CompiledShader
    {
        std::vector<std::byte> DxilBlob;
        std::vector<std::byte> PdbBlob;

        std::unordered_set<std::filesystem::path> IncludeFilePaths;

        bool IsValid() const { return !DxilBlob.empty(); }
    };

    class DxcShaderCompiler
    {
    public:
        DxcShaderCompiler();
        ~DxcShaderCompiler();

        BenzinDefineNonCopyable(DxcShaderCompiler);
        BenzinDefineNonMoveable(DxcShaderCompiler);

        CompiledShader CompileShader(const ShaderPaths& paths, const ShaderArgs& args) const;

    private:
        class IncludeHandler;

        ComPtr<IDxcResult> GetDxcCompileResult(const ShaderPaths& paths, const ShaderArgs& args, CompiledShader& outCompiledShader) const;
        void ParseDxcCompileResult(const ComPtr<IDxcResult>& dxcResult, CompiledShader& outCompiledShader) const;

        ComPtr<IDxcUtils> m_DxcUtils;
        ComPtr<IDxcCompiler3> m_DxcCompiler;

        std::unique_ptr<IncludeHandler> m_IncludeHandler;
    };

}
