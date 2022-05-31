#pragma once

#include "spieler/core/assert.hpp"

#include "spieler/renderer/descriptor_manager.hpp"
#include "spieler/renderer/upload_buffer.hpp"

namespace spieler::renderer
{

    class ConstantBuffer;

    class ConstantBufferSlice
    {
    public:
        ConstantBufferSlice() = default;
        ConstantBufferSlice(ConstantBuffer* constantBuffer, uint32_t offset);

    public:
        void Bind(Context& context, uint32_t rootParameterIndex) const;
        void Update(const void* data, uint32_t size);

    private:
        ConstantBuffer* m_ConstantBuffer{ nullptr };
        uint32_t m_Offset{ 0 };
    };

    class ConstantBuffer
    {
    public:
        friend class ConstantBufferSlice;

    public:
        void SetResource(const std::shared_ptr<BufferResource>& resource);

        bool HasSlice(const void* key) const;

        ConstantBufferSlice& GetSlice(const void* key);
        const ConstantBufferSlice& GetSlice(const void* key) const;

        void SetSlice(const void* key);

    private:
        std::shared_ptr<BufferResource> m_Resource;
        std::unordered_map<const void*, ConstantBufferSlice> m_Slices;
    };

} // namespace spieler::renderer