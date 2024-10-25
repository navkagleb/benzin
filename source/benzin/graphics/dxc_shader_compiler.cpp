#include "benzin/config/bootstrap.hpp"
#include "benzin/graphics/dxc_shader_compiler.hpp"

#include "benzin/core/asserter.hpp"
#include "benzin/core/logger.hpp"
#include "benzin/graphics/pipeline_state.hpp"

namespace benzin
{

    static constexpr auto g_ShaderTargets = []
    {
        EnumArray<std::wstring_view, ShaderType> shaderTargets;
        shaderTargets[+ShaderType::Vertex] = L"vs_6_6";
        shaderTargets[+ShaderType::Pixel] = L"ps_6_6";
        shaderTargets[+ShaderType::Compute] = L"cs_6_6";
        shaderTargets[+ShaderType::Library] = L"lib_6_6";

        return shaderTargets;
    }();

    static std::vector<const wchar_t*> GetCompileArgs(const ShaderPaths& paths, const ShaderArgs& args)
    {
        std::vector<const wchar_t*> compileArgs;

        // Fill paths
        {
            // Source file
            compileArgs.push_back(paths.SourceFilePath.c_str());

            // Include directory
            compileArgs.push_back(L"-I");
            compileArgs.push_back(GfxConfig::s_ShaderSourceDir.c_str());

            // Optimizations
            compileArgs.push_back(GfxConfig::s_IsShaderDebugEnabled ? DXC_ARG_SKIP_OPTIMIZATIONS : DXC_ARG_OPTIMIZATION_LEVEL3);

            if (GfxConfig::s_IsShaderSymbolsEnabled)
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

            // Defines
            for (const auto& define : args.Defines)
            {
                compileArgs.push_back(L"-D");
                compileArgs.push_back(define.c_str());
            }
        }

        compileArgs.push_back(L"-HV 2021");

        return compileArgs;
    }

    // ShaderPaths

    ShaderPaths::ShaderPaths(const ShaderInfo& shader)
        : SourceFilePath{ GfxConfig::s_ShaderSourceDir / shader.GetFileName() }
        , DxilFilePath{ GfxConfig::s_ShaderDxilDir / std::format("{}.bin", shader.GetHash()) }
        , PdbFilePath{ GfxConfig::s_ShaderPdbDir / std::format("{}.pdb", shader.GetHash()) }
    {
        BenzinEnsure(std::filesystem::exists(SourceFilePath));

        SourceFilePath.make_preferred();
        DxilFilePath.make_preferred();
        PdbFilePath.make_preferred();
    }

    // ShaderArgs

    ShaderArgs::ShaderArgs(const ShaderInfo& shader)
        : Target{ g_ShaderTargets[+shader.GetType()]}
        , EntryPoint{ ToWideString(shader.GetEntryPoint()) }
        , Defines{ std::from_range, shader.GetDefines() | std::views::transform(ToWideString) }
    {}

    // DxcShaderCompiler::IncludeHandler

    class DxcShaderCompiler::IncludeHandler : public IDxcIncludeHandler
    {
    public:
        explicit IncludeHandler(IDxcUtils* dxcUtils)
            : m_DxcUtils{ dxcUtils }
        {}

        auto&& GetMovedIncludeFilePathes()
        {
            return std::move(m_IncludeFilePaths);
        }

        void ClearIncludeFilePathes()
        {
            m_IncludeFilePaths.clear();
        }

        // IDxcIncludeHandler interface
        HRESULT LoadSource(LPCWSTR rawAbsFilePath,  IDxcBlob** outDxcIncludeSource) override
        {
            // Ref: https://simoncoenen.com/blog/programming/graphics/DxcCompiling#custom-include-handler

            static const auto getShaderIncludeFilePath = [](std::wstring_view includeFilePathToParse)
            {
                // Retrieves include file like in a shader
                // Assume that all file names starts from GfxConfig::s_ShaderSourceDir
                // 'includeFilePathToParse' has backslashes ('\') which is adds by DxcCompiler
                // include file path which used in shader uses normal slash ('/')

                std::filesystem::path includeFilePath;

                while (true)
                {
                    const size_t slashPos = includeFilePathToParse.find_first_of('/');
                    BenzinEnsure(slashPos != std::wstring_view::npos, "Invalid include file");

                    includeFilePathToParse = includeFilePathToParse.substr(slashPos + 1);

                    includeFilePath = GfxConfig::s_ShaderSourceDir / includeFilePathToParse;
                    if (std::filesystem::exists(includeFilePath.make_preferred()))
                    {
                        break;
                    }
                }

                return includeFilePath;
            };

            auto includeFilePath = getShaderIncludeFilePath(rawAbsFilePath);

            ComPtr<IDxcBlobEncoding> dxcIncludeSource;

            if (m_IncludeFilePaths.contains(includeFilePath))
            {
                static const std::string_view nullStr = " ";

                m_DxcUtils->CreateBlobFromPinned(nullStr.data(), (uint32_t)nullStr.size(), DXC_CP_ACP, dxcIncludeSource.GetAddressOf());
                *outDxcIncludeSource = dxcIncludeSource.Detach();

                return S_OK;
            }

            const HRESULT hr = m_DxcUtils->LoadFile(includeFilePath.c_str(), nullptr, dxcIncludeSource.GetAddressOf());
            if (SUCCEEDED(hr))
            {
                m_IncludeFilePaths.insert(std::move(includeFilePath));
                *outDxcIncludeSource = dxcIncludeSource.Detach();
            }

            return hr;
        }

        // IUnknown interface
        HRESULT STDMETHODCALLTYPE QueryInterface(REFIID, _COM_Outptr_ void __RPC_FAR* __RPC_FAR*) override { return E_NOINTERFACE; }
        ULONG STDMETHODCALLTYPE AddRef() override { return 0; }
        ULONG STDMETHODCALLTYPE Release() override { return 0; }

    private:
        IDxcUtils* m_DxcUtils = nullptr;

        std::unordered_set<std::filesystem::path> m_IncludeFilePaths;
    };

    // DxcShaderCompiler

    DxcShaderCompiler::DxcShaderCompiler()
    {
        BenzinEnsure(::DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&m_DxcUtils)));
        BenzinEnsure(::DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&m_DxcCompiler)));

        MakeUniquePtr(m_IncludeHandler, m_DxcUtils.Get());
    }

    DxcShaderCompiler::~DxcShaderCompiler() = default;

    CompiledShader DxcShaderCompiler::CompileShader(const ShaderPaths& paths, const ShaderArgs& args) const
    {
        CompiledShader compiledShader;
        const auto dxcCompileResult = GetDxcCompileResult(paths, args, compiledShader);
        if (dxcCompileResult)
        {
            ParseDxcCompileResult(dxcCompileResult, compiledShader);
        }

        return compiledShader;
    }

    ComPtr<IDxcResult> DxcShaderCompiler::GetDxcCompileResult(const ShaderPaths& paths, const ShaderArgs& args, CompiledShader& outCompiledShader) const
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
            m_IncludeHandler.get(),
            IID_PPV_ARGS(&dxcResult)
        ));

        ComPtr<IDxcBlobUtf8> dxcErrorBlob;
        ComPtr<IDxcBlobWide> dummyName; // To avoid warning
        dxcResult->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&dxcErrorBlob), &dummyName);

        if (dxcErrorBlob && dxcErrorBlob->GetStringLength() > 0)
        {
            m_IncludeHandler->ClearIncludeFilePathes();

            BenzinError("Failed to compile shader: {} ({})", paths.SourceFilePath.string(), ToNarrowString(args.EntryPoint));
            BenzinError("ErrorMessage: \n{}", dxcErrorBlob->GetStringPointer());

            return nullptr;
        }

        outCompiledShader.IncludeFilePaths = m_IncludeHandler->GetMovedIncludeFilePathes();

        return dxcResult;
    }

    void DxcShaderCompiler::ParseDxcCompileResult(const ComPtr<IDxcResult>& dxcResult, CompiledShader& outCompiledShader) const
    {
        {
            ComPtr<IDxcBlob> dxcBinaryBlob;
            BenzinEnsure(dxcResult->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&dxcBinaryBlob), nullptr));
            BenzinAssert(dxcBinaryBlob.Get() && dxcBinaryBlob->GetBufferSize() != 0);

            const auto* data = reinterpret_cast<const std::byte*>(dxcBinaryBlob->GetBufferPointer());
            const size_t size = dxcBinaryBlob->GetBufferSize();

            outCompiledShader.DxilBlob.assign(data, data + size);
        }

        if (GfxConfig::s_IsShaderSymbolsEnabled)
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
