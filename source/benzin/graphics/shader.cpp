#include "benzin/config/bootstrap.hpp"
#include "benzin/graphics/shader.hpp"

namespace benzin
{

    ShaderInfo::ShaderInfo(ShaderType type, std::string_view fileName, std::string_view entryPoint, std::vector<std::string_view>&& defines)
        : m_Type{ type }
        , m_FileName{ fileName }
        , m_EntryPoint{ entryPoint }
        , m_Defines{ std::move(defines) }
        , m_Hash{ 0 }
    {
        m_Hash = HashCombine(m_Hash, +m_Type);
        m_Hash = HashCombine(m_Hash, m_FileName);
        m_Hash = HashCombine(m_Hash, m_EntryPoint);

        for (auto define : m_Defines)
        {
            m_Hash = HashCombine(m_Hash, define);
        }
    }

    bool ShaderInfo::IsValid() const
    {
        if (m_Type == ShaderType::Library)
            return !m_FileName.empty();

        return !m_FileName.empty() && !m_EntryPoint.empty();
    }

}
