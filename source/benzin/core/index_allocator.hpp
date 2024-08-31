#pragma once

namespace benzin
{

    class IndexAllocator
    {
    public:
        explicit IndexAllocator(uint32_t maxIndexCount);
        ~IndexAllocator();

    public:
        auto GetMaxIndexCount() const { return m_MaxIndexCount; }
        auto GetAllocatedIndexCount() const { return m_AllocatedIndexCount; }

        uint32_t AllocateIndex();
        void FreeIndex(uint32_t index);

    private:
        struct SplittedIndex
        {
            uint32_t ChunkIndex = 0;
            uint32_t ChunkBitIndex = 0;

            explicit SplittedIndex(uint32_t index);
        };

        static constexpr uint64_t ms_DefaultChunkValue = std::numeric_limits<uint64_t>::max();
        static constexpr uint32_t ms_ChunkBitCount = sizeof(ms_DefaultChunkValue) * 8;

        std::vector<std::bitset<ms_ChunkBitCount>> m_ChunkBits;
        uint32_t m_MaxIndexCount = 0;
        uint32_t m_FreeIndex = 0;

        uint32_t m_AllocatedIndexCount = 0;
    };

}
