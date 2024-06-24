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

    class Dxc_CustomIncludeHandler : public IDxcIncludeHandler
    {
    public:
        explicit Dxc_CustomIncludeHandler(IDxcUtils* dxcUtils);

        void ExchangeIncludeFilePaths(std::unordered_set<std::filesystem::path>& outIncludeFilePathes);

        // IDxcIncludeHandler interface

        HRESULT STDMETHODCALLTYPE LoadSource(_In_z_ LPCWSTR fileName, _COM_Outptr_result_maybenull_ IDxcBlob** includeSource) override;

        // IUnknown interface

        HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, _COM_Outptr_ void __RPC_FAR* __RPC_FAR* ppvObject) override;
        ULONG STDMETHODCALLTYPE AddRef() override;
        ULONG STDMETHODCALLTYPE Release() override;

    private:
        IDxcUtils* m_DxcUtils = nullptr;

        std::vector<std::filesystem::path> m_IncludeFilePaths;
    };

    class Dxc_ShaderCompiler
    {
    public:
        static const std::filesystem::path& GetShaderSourceDir();

    public:
        Dxc_ShaderCompiler();

        BenzinDefineNonCopyable(Dxc_ShaderCompiler);
        BenzinDefineNonMoveable(Dxc_ShaderCompiler);

        CompiledShader CompileShader(const ShaderPaths& paths, const ShaderArgs& args) const;

    private:
        ComPtr<IDxcResult> GetDxcCompileResult(const ShaderPaths& paths, const ShaderArgs& args, CompiledShader& outCompiledShader) const;
        void ParseDxcCompileResult(const ComPtr<IDxcResult>& dxcResult, CompiledShader& outCompiledShader) const;

    private:
        ComPtr<IDxcUtils> m_DxcUtils;
        ComPtr<IDxcCompiler3> m_DxcCompiler;

        std::unique_ptr<Dxc_CustomIncludeHandler> m_CustomIncludeHandler;
    };

}
