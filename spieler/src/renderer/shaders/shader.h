#pragma once

#include <string>

#include "renderer/renderer_object.h"

namespace Spieler
{

    class Shader : public RendererObject
    {
    public:
        virtual bool LoadFromFile(const std::wstring& filename, const std::string& entryPoint) = 0;

    public:
        const void* GetData() const { return m_ByteCode->GetBufferPointer(); }
        std::uint32_t GetSize() const { return static_cast<std::uint32_t>(m_ByteCode->GetBufferSize()); }

    protected:
        ComPtr<ID3DBlob> m_ByteCode;
    };

    class VertexShader : public Shader
    {
    public:
        bool LoadFromFile(const std::wstring& filename, const std::string& entryPoint) override; 
    };

    class PixelShader : public Shader
    {
    public:
        bool LoadFromFile(const std::wstring& filename, const std::string& entryPoint) override;
    };

} // namespace Spieler