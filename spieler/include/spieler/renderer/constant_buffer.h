#pragma once

#include <variant>

#include "upload_buffer.h"

namespace Spieler
{

    class DescriptorHeap;

    class ConstantBuffer : public RendererObject
    {
    public:
        template <typename T>
        bool InitAsRootDescriptorTable(UploadBuffer* uploadBuffer, std::uint32_t index, const DescriptorHeap& heap, std::uint32_t heapIndex);

        template <typename T>
        bool InitAsRootDescriptor(UploadBuffer* uploadBuffer, std::uint32_t index);

        template <typename T>
        T& As();

        template <typename T>
        const T& As() const;

        void BindAsRootDescriptorTable(const DescriptorHeap& heap) const;
        void BindAsRootDescriptor(std::uint32_t registerIndex) const;

    private:
        template <typename T>
        bool Init(UploadBuffer* uploadBuffer, std::uint32_t index);

    private:
        UploadBuffer*   m_UploadBuffer{ nullptr };
        std::uint32_t   m_Index{ 0   };
        
        // Only for RootDescriptorTable
        std::uint32_t   m_HeapIndex{ 0 };
    };

    template <typename T>
    T& ConstantBuffer::As() 
    { 
        SPIELER_ASSERT(m_UploadBuffer);

        return (*m_UploadBuffer).As<T>(m_Index);
    }

    template <typename T>
    const T& ConstantBuffer::As() const 
    { 
        SPIELER_ASSERT(m_UploadBuffer);

        return (*m_UploadBuffer).As<T>(m_Index);
    }

} // namespace Spieler

#include "constant_buffer.inl"