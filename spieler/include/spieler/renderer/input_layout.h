#pragma once

#include <vector>

#include <d3d12.h>

#include "renderer_object.h"

namespace Spieler
{

    struct InputLayoutElement
    {
        std::string     Name;
        DXGI_FORMAT     Format  = DXGI_FORMAT_UNKNOWN;
        std::uint32_t   Size    = 0;
    };

    class InputLayout : public RendererObject
    {
    public:
        bool Init(Renderer& renderer, const InputLayoutElement* elements, std::uint32_t count)
        {
            m_InputElements.reserve(count);

            std::uint32_t offset = 0;

            for (std::uint32_t i = 0; i < count; ++i)
            {
                const InputLayoutElement& element = elements[i];

                m_InputElements.push_back(D3D12_INPUT_ELEMENT_DESC{
                    element.Name.c_str(), 
                    0, 
                    element.Format, 
                    0, 
                    offset, 
                    D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 
                    0 
                });

                offset += element.Size;
            }

            return true;
        }

        const D3D12_INPUT_ELEMENT_DESC* GetData() const
        {
            return m_InputElements.data();
        }

        std::uint32_t GetCount() const
        {
            return static_cast<std::uint32_t>(m_InputElements.size());
        }

    private:
        std::vector<D3D12_INPUT_ELEMENT_DESC> m_InputElements;
    };

} // namespace Spieler