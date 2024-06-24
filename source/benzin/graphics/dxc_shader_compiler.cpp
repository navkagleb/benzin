#include "benzin/config/bootstrap.hpp"
#include "benzin/graphics/dxc_shader_compiler.hpp"

#include "benzin/core/asserter.hpp"
#include "benzin/core/logger.hpp"

namespace benzin
{

    static const auto g_ShaderSourceDir = std::filesystem::absolute("source/shaders");
    static const auto g_ShaderDxilDir = std::filesystem::absolute(
#if BENZIN_IS_DEBUG_BUILD
        "bin/shader_dxil_debug"
#elif BENZIN_IS_RELEASE_BUILD
        "bin/shader_dxil_release"
#else
    #error
#endif
    );
    static const auto g_ShaderPdbDir = std::filesystem::absolute("bin/shader_pbd");

    static constexpr auto GetShaderTargets()
    {
        EnumArray<std::wstring_view, ShaderType> shaderTargets;
        shaderTargets[+ShaderType::Vertex] = L"vs_6_6";
        shaderTargets[+ShaderType::Pixel] = L"ps_6_6";
        shaderTargets[+ShaderType::Compute] = L"cs_6_6";
        shaderTargets[+ShaderType::Library] = L"lib_6_6";

        return shaderTargets;
    }

    static constexpr auto g_ShaderTargets = GetShaderTargets();

    static std::vector<const wchar_t*> GetCompileArgs(const ShaderPaths& paths, const ShaderArgs& args)
    {
        std::vector<const wchar_t*> compileArgs;

        // Fill paths
        {
            // Source file
            compileArgs.push_back(paths.SourceFilePath.c_str());

            // Include directory
            compileArgs.push_back(L"-I");
            compileArgs.push_back(g_ShaderSourceDir.c_str());

            // Optimizations
            compileArgs.push_back(config::g_IsShaderDebugEnabled ? DXC_ARG_SKIP_OPTIMIZATIONS : DXC_ARG_OPTIMIZATION_LEVEL3);

            if constexpr (config::g_IsShaderSymbolsEnabled)
            {
                compileArgs.push_back(DXC_ARG_DEBUG); // Generate symbols

                // PDB file
                compileArgs.push_back(L"-Fd");
                compileArgs.push_back(paths.PdbFilePath.c_str());
            }

            // Binary file
            compileArgs.push_back(L"-Fo");
            compileArgs.push_back(paths.DxilFilePath.c_str());

            // Remove everything from binary
            compileArgs.push_back(L"-Qstrip_debug");
            compileArgs.push_back(L"-Qstrip_reflect");
            compileArgs.push_back(L"-Qstrip_rootsignature");

            // Other arguments
            compileArgs.push_back(DXC_ARG_WARNINGS_ARE_ERRORS);
            compileArgs.push_back(DXC_ARG_PACK_MATRIX_ROW_MAJOR); // To prevent matrix transpose in CPU side
        }

        // Fill args
        {
            // Target
            BenzinAssert(!args.Target.empty());
            compileArgs.push_back(L"-T");
            compileArgs.push_back(args.Target.data());

            if (!args.EntryPoint.empty())
            {
                // Entry point
                compileArgs.push_back(L"-E");
                compileArgs.push_back(args.EntryPoint.c_str());
            }

#if 0
            // Defines
            for (const auto& define : args.Defines)
            {
                compileArgs.push_back(L"-D");
                compileArgs.push_back(define.c_str());
            }
#endif
        }

        return compileArgs;
    }

    // ShaderPaths

    ShaderPaths::ShaderPaths(size_t hash, std::string_view fileName)
        : SourceFilePath{ g_ShaderSourceDir / fileName }
        , DxilFilePath{ g_ShaderDxilDir / std::format("{}.bin", hash) }
        , PdbFilePath{ g_ShaderPdbDir / std::format("{}.pdb", hash) }
    {
        BenzinEnsure(std::filesystem::exists(SourceFilePath));
    }

    // ShaderArgs

    ShaderArgs::ShaderArgs(ShaderType shaderType, std::string_view entryPoint)
        : Target{ g_ShaderTargets[+shaderType] }
        , EntryPoint{ ToWideString(entryPoint) }
    {}

    // Dxc_CustomIncludeHandler

    Dxc_CustomIncludeHandler::Dxc_CustomIncludeHandler(IDxcUtils* dxcUtils)
        : m_DxcUtils{ dxcUtils }
    {}

    void Dxc_CustomIncludeHandler::ExchangeIncludeFilePaths(std::unordered_set<std::filesystem::path>& outIncludeFilePathes)
    {
        outIncludeFilePathes.insert_range(std::exchange(m_IncludeFilePaths, {}));
    }

    HRESULT STDMETHODCALLTYPE Dxc_CustomIncludeHandler::LoadSource(_In_z_ LPCWSTR pFilename, _COM_Outptr_result_maybenull_ IDxcBlob** outIncludeSource)
    {
        // Ref: https://simoncoenen.com/blog/programming/graphics/DxcCompiling#custom-include-handler

        std::filesystem::path includeFilePath{ pFilename };
        BenzinAssert(std::filesystem::exists(includeFilePath));

        ComPtr<IDxcBlobEncoding> includeSource;
        const HRESULT hr = m_DxcUtils->LoadFile(includeFilePath.c_str(), nullptr, &includeSource);

        if (SUCCEEDED(hr))
        {
            m_IncludeFilePaths.push_back(std::move(includeFilePath));

            *outIncludeSource = includeSource.Detach();
        }

        return hr;
    }

    HRESULT STDMETHODCALLTYPE Dxc_CustomIncludeHandler::QueryInterface(REFIID riid, _COM_Outptr_ void __RPC_FAR* __RPC_FAR* ppvObject)
    {
        BenzinUnused(riid);
        BenzinUnused(ppvObject);

        return E_NOINTERFACE;
    }

    ULONG STDMETHODCALLTYPE Dxc_CustomIncludeHandler::AddRef()
    {
        return 0;
    }

    ULONG STDMETHODCALLTYPE Dxc_CustomIncludeHandler::Release()
    {
        return 0;
    }

    // DxcShaderCompiler

    const std::filesystem::path& Dxc_ShaderCompiler::GetShaderSourceDir()
    {
        return g_ShaderSourceDir;
    }

    Dxc_ShaderCompiler::Dxc_ShaderCompiler()
    {
        BenzinEnsure(::DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&m_DxcUtils)));
        BenzinEnsure(::DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&m_DxcCompiler)));

        MakeUniquePtr(m_CustomIncludeHandler, m_DxcUtils.Get());
    }

    CompiledShader Dxc_ShaderCompiler::CompileShader(const ShaderPaths& paths, const ShaderArgs& args) const
    {
        CompiledShader compiledShader;
        const auto dxcCompileResult = GetDxcCompileResult(paths, args, compiledShader);
        if (dxcCompileResult)
        {
            ParseDxcCompileResult(dxcCompileResult, compiledShader);
        }

        return compiledShader;
    }

    ComPtr<IDxcResult> Dxc_ShaderCompiler::GetDxcCompileResult(const ShaderPaths& paths, const ShaderArgs& args, CompiledShader& outCompiledShader) const
    {
        uint32_t codePage = CP_UTF8;
        ComPtr<IDxcBlobEncoding> dxcShaderSource;
        BenzinEnsure(m_DxcUtils->LoadFile(paths.SourceFilePath.c_str(), &codePage, &dxcShaderSource));

        const DxcBuffer dxcSourceBuffer
        {
            .Ptr = dxcShaderSource->GetBufferPointer(),
            .Size = dxcShaderSource->GetBufferSize(),
            .Encoding = 0,
        };

        const std::vector<const wchar_t*> compileArgs = GetCompileArgs(paths, args);

        ComPtr<IDxcResult> dxcResult;
        BenzinEnsure(m_DxcCompiler->Compile(
            &dxcSourceBuffer,
            (LPCWSTR*)compileArgs.data(),
            (uint32_t)compileArgs.size(),
            m_CustomIncludeHandler.get(),
            IID_PPV_ARGS(&dxcResult)
        ));

        ComPtr<IDxcBlobUtf8> dxcErrorBlob;
        ComPtr<IDxcBlobWide> dummyName; // To avoid warning
        dxcResult->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&dxcErrorBlob), &dummyName);

        if (dxcErrorBlob && dxcErrorBlob->GetStringLength() > 0)
        {
            BenzinError("Failed to compile shader: {} ({})", paths.SourceFilePath.string(), ToNarrowString(args.EntryPoint));
            BenzinError("ErrorMessage: \n{}", dxcErrorBlob->GetStringPointer());

            return nullptr;
        }

        m_CustomIncludeHandler->ExchangeIncludeFilePaths(outCompiledShader.IncludeFilePaths);

        return dxcResult;
    }

    void Dxc_ShaderCompiler::ParseDxcCompileResult(const ComPtr<IDxcResult>& dxcResult, CompiledShader& outCompiledShader) const
    {
        {
            ComPtr<IDxcBlob> dxcBinaryBlob;
            BenzinEnsure(dxcResult->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&dxcBinaryBlob), nullptr));
            BenzinAssert(dxcBinaryBlob.Get() && dxcBinaryBlob->GetBufferSize() != 0);

            const auto* data = reinterpret_cast<const std::byte*>(dxcBinaryBlob->GetBufferPointer());
            const size_t size = dxcBinaryBlob->GetBufferSize();

            outCompiledShader.DxilBlob.assign(data, data + size);
        }

        if constexpr (config::g_IsShaderSymbolsEnabled)
        {
            ComPtr<IDxcBlob> dxcDebugBlob;
            BenzinEnsure(dxcResult->GetOutput(DXC_OUT_PDB, IID_PPV_ARGS(&dxcDebugBlob), nullptr));
            BenzinAssert(dxcDebugBlob.Get() && dxcDebugBlob->GetBufferPointer());

            const auto* data = reinterpret_cast<const std::byte*>(dxcDebugBlob->GetBufferPointer());
            const size_t size = dxcDebugBlob->GetBufferSize();

            outCompiledShader.PdbBlob.assign(data, data + size);
        }
    }

}
