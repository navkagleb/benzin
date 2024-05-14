#include "benzin/config/bootstrap.hpp"
#include "benzin/graphics/shader_manager.hpp"

#include "benzin/core/asserter.hpp"
#include "benzin/core/logger.hpp"
#include "benzin/utility/time_utils.hpp"

namespace benzin
{

    static size_t GetShaderHash(ShaderType shaderType, std::string_view fileName, std::string_view entryPoint)
    {
        size_t hash = 0;
        hash = HashCombine(hash, magic_enum::enum_integer(shaderType));
        hash = HashCombine(hash, fileName);
        hash = HashCombine(hash, entryPoint);

        return hash;
    }

    //

    std::span<const std::byte> ShaderManager::GetShaderDxil(ShaderType shaderType, std::string_view fileName, std::string_view entryPoint) const
    {
        const size_t shaderHash = GetShaderHash(shaderType, fileName, entryPoint);
        
        const auto it = m_ShaderDxils.find(shaderHash);
        if (it != m_ShaderDxils.end())
        {
            return it->second;
        }

        const ShaderPaths paths{ shaderHash, fileName };
        const ShaderArgs args{ shaderType, entryPoint };
        const auto [us, compileResult] = BenzinProfileFunction(m_DxcShaderCompiler.CompileShader(paths, args));

        if (shaderType == ShaderType::Library)
        {
            BenzinTrace("LibraryCompiled: {}! File: {}, Time: {} ms", shaderHash, fileName, ToFloatMs(us));
        }
        else
        {
            BenzinTrace("ShaderCompiled: {}! File: {}, EntryPoint: {}. Time: {} ms", shaderHash, fileName, entryPoint, ToFloatMs(us));
        }

        WriteToFile(paths.DxilFilePath, compileResult.DxilBlob);

        if constexpr (config::g_IsShaderSymbolsEnabled)
        {
            BenzinAssert(!compileResult.PdbBlob.empty());
            WriteToFile(paths.PdbFilePath, compileResult.PdbBlob);
        }

        auto& shaderDxil = m_ShaderDxils[shaderHash];
        shaderDxil = std::move(compileResult.DxilBlob);

        return shaderDxil;
    }

    std::span<const std::byte> ShaderManager::GetLibraryDxil(std::string_view fileName) const
    {
        return GetShaderDxil(ShaderType::Library, fileName, "");
    }

}
