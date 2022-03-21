#pragma once

#include <string>
#include <vector>

#include "renderer_object.h"

namespace Spieler
{

    struct ShaderDefine
    {
        std::string Name;
        std::string Value;
    };

    class Shader : public RendererObject
    {
    public:
        virtual bool LoadFromFile(const std::wstring& filename, const std::string& entryPoint) = 0;
        virtual bool LoadFromFile(const std::wstring& filename, const std::string& entryPoint, const std::vector<ShaderDefine>& defines) = 0;

    public:
        const void* GetData() const;
        std::uint32_t GetSize() const;

    protected:
        ComPtr<ID3DBlob> m_ByteCode;
    };

    class VertexShader : public Shader
    {
    public:
        bool LoadFromFile(const std::wstring& filename, const std::string& entryPoint) override; 
        bool LoadFromFile(const std::wstring& filename, const std::string& entryPoint, const std::vector<ShaderDefine>& defines) override;
    };

    class PixelShader : public Shader
    {
    public:
        bool LoadFromFile(const std::wstring& filename, const std::string& entryPoint) override;
        bool LoadFromFile(const std::wstring& filename, const std::string& entryPoint, const std::vector<ShaderDefine>& defines) override;
    };

} // namespace Spieler