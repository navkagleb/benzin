#pragma once

#include "spieler/core/assert.hpp"
#include "spieler/core/common.hpp"

#include "spieler/renderer/resource.hpp"
#include "spieler/renderer/device.hpp"

namespace spieler::renderer
{

    enum class UploadBufferType : uint32_t
    {
        Default = 0,
        ConstantBuffer = 1
    };

    class UploadBuffer : public Resource
    {
    public:
        UploadBuffer() = default;

        template <typename T = std::byte>
        UploadBuffer(Device& device, UploadBufferType type, uint32_t elementCount);

        UploadBuffer(UploadBuffer&& other) noexcept;
        ~UploadBuffer();

    public:
        uint32_t GetSize() const { return m_Size; }
        uint32_t GetStride() const { return m_Stride; }
        uint32_t GetMarker() const { return m_Marker; }

    public:
        template <typename T = std::byte>
        bool Init(Device& device, UploadBufferType type, uint32_t elementCount);

    public:
        uint32_t Allocate(uint32_t size, uint32_t alignment = 0);

        GPUVirtualAddress GetGPUVirtualAddress() const { return static_cast<GPUVirtualAddress>(m_Resource->GetGPUVirtualAddress()); }
        GPUVirtualAddress GetElementGPUVirtualAddress(uint32_t index) const { return static_cast<GPUVirtualAddress>(m_Resource->GetGPUVirtualAddress() + static_cast<uint64_t>(index * m_Stride)); }

        template <typename T>
        T& As(uint32_t index = 0);

    public:
        UploadBuffer& operator =(UploadBuffer&& other) noexcept;

    private:
        std::byte* m_MappedData{ nullptr };
        uint32_t m_Size{ 0 };
        uint32_t m_Stride{ 0 };
        uint32_t m_Marker{ 0 };
    };

    namespace _internal
    {

        inline uint32_t CalcConstantBufferSize(uint32_t size)
        {
            return (size + 255) & ~255;
        }

    } // namespace _internal

    template <typename T>
    UploadBuffer::UploadBuffer(Device& device, UploadBufferType type, uint32_t elementCount)
    {
        SPIELER_ASSERT(Init<T>(device, type, elementCount));
    }

    template <typename T>
    bool UploadBuffer::Init(Device& device, UploadBufferType type, uint32_t elementCount)
    {
        SPIELER_ASSERT(elementCount != 0);

        m_Stride = type == UploadBufferType::ConstantBuffer ? _internal::CalcConstantBufferSize(sizeof(T)) : sizeof(T);
        m_Size = m_Stride * elementCount;
        
        const BufferConfig bufferConfig
        {
            .Alignment = 0,
            .ElementSize = m_Stride,
            .ElementCount = elementCount
        };

        SPIELER_RETURN_IF_FAILED(device.CreateUploadBuffer(bufferConfig, *this));
        SPIELER_RETURN_IF_FAILED(m_Resource->Map(0, nullptr, reinterpret_cast<void**>(&m_MappedData)));

        return true;
    }

    template <typename T>
    T& UploadBuffer::As(uint32_t index)
    {
        SPIELER_ASSERT(index * m_Stride < m_Size);

        return *reinterpret_cast<T*>(&m_MappedData[index * m_Stride]);
    }

} // namespace spieler::renderer