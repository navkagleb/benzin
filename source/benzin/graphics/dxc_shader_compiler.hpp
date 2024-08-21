#pragma once

#include "benzin/graphics/common.hpp"

namespace benzin
{

    struct ShaderPaths
    {
        std::filesystem::path SourceFilePath;
        std::filesystem::path DxilFilePath;
        std::filesystem::path PdbFilePath;

        ShaderPaths(size_t hash, std::string_view fileName);
    };

    struct ShaderArgs
    {
        std::wstring_view Target;
        std::wstring EntryPoint;

        explicit ShaderArgs(ShaderType shaderType, std::string_view entryPoint = {});
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
        ComPtr<IDxcResult> GetDxcCompileResult(const ShaderPaths& paths, const ShaderArgs& args, CompiledShader& outCompiledShader) const;
        void ParseDxcCompileResult(const ComPtr<IDxcResult>& dxcResult, CompiledShader& outCompiledShader) const;

    private:
        class IncludeHandler;

        ComPtr<IDxcUtils> m_DxcUtils;
        ComPtr<IDxcCompiler3> m_DxcCompiler;

        std::unique_ptr<IncludeHandler> m_IncludeHandler;
    };

}
