#pragma once

#include <variant>

#include "renderer_object.hpp"
#include "upload_buffer.hpp"

namespace Spieler
{

    class DescriptorHeap;

    class ConstantBuffer : public RendererObject
    {
    public:
        friend class ConstantBufferView;

    public:
        ConstantBuffer() = default;
        ConstantBuffer(UploadBuffer& uploadBuffer, std::uint32_t index);

    public:
        void Init(UploadBuffer& uploadBuffer, std::uint32_t index);

        void Bind(std::uint32_t rootParameterIndex) const;

        template <typename T>
        T& As();

        template <typename T>
        const T& As() const;

    private:
        UploadBuffer* m_UploadBuffer{ nullptr };
        std::uint32_t m_Index{ 0 };
    };

    class ConstantBufferView : public RendererObject
    {
    public:
        ConstantBufferView() = default;
        ConstantBufferView(const ConstantBuffer& constantBuffer, const DescriptorHeap& descriptorHeap, std::uint32_t descriptorHeapIndex);

    public:
        void Init(const ConstantBuffer& constantBuffer, const DescriptorHeap& descriptorHeap, std::uint32_t descriptorHeapIndex);

        void Bind(std::uint32_t rootParameterIndex) const;

    private:
        const DescriptorHeap* m_DescriptorHeap{ nullptr };
        std::uint32_t m_DescriptorHeapIndex{ 0 };
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