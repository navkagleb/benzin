#pragma once

#include <variant>

#include "renderer_object.h"

namespace Spieler
{

    class DescriptorHeap;

    class ConstantBuffer : public RendererObject
    {
    public:
        ConstantBuffer() = default;
        ConstantBuffer(ConstantBuffer&& other) noexcept;

    public:
        ~ConstantBuffer();

    public:
        template <typename T>
        bool InitAsRootDescriptorTable(const DescriptorHeap& heap, std::uint32_t index);

        template <typename T>
        bool InitAsRootDescriptorTable(const T& data, const DescriptorHeap& heap, std::uint32_t index);

        template <typename T>
        bool InitAsRootDescriptor();

        template <typename T>
        bool InitAsRootDescriptor(const T& data);

        template <typename T>
        T& As() { return *reinterpret_cast<T*>(m_MappedData); }

        template <typename T>
        const T& As() const { return *reinterpret_cast<T*>(m_MappedData); }

        void BindAsRootDescriptorTable(const DescriptorHeap& heap) const;
        void BindAsRootDescriptor(std::uint32_t registerIndex) const;

    private:
        template <typename T>
        bool Init(const T& data);

    private:
        ComPtr<ID3D12Resource>  m_UploadBuffer;
        void*                   m_MappedData{ nullptr };
        
        // Only for RootDescriptorTable
        std::uint32_t           m_Index{ 0 };
    };

} // namespace Spieler

#include "constant_buffer.inl"