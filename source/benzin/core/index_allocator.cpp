#include "benzin/config/bootstrap.hpp"
#include "benzin/core/index_allocator.hpp"

#include "benzin/core/math.hpp"

namespace benzin
{

    // IndexAllocator::SplittedIndex

    IndexAllocator::SplittedIndex::SplittedIndex(uint32_t index)
        : ChunkIndex{ index >> FindPowerOf2(ms_ChunkBitCount) }
        , ChunkBitIndex{ index & (ms_ChunkBitCount - 1) }
    {}

    // IndexAllocator

    IndexAllocator::IndexAllocator(uint32_t maxIndexCount)
        : m_MaxIndexCount{ maxIndexCount }
    {}

    IndexAllocator::~IndexAllocator()
    {
        BenzinAssert(m_AllocatedIndexCount == 0);
    }

    uint32_t IndexAllocator::AllocateIndex()
    {
        while (m_FreeIndex < m_MaxIndexCount)
        {
            const SplittedIndex splittedIndex{ m_FreeIndex };

            if (splittedIndex.ChunkIndex == m_ChunkBits.size())
            {
                m_ChunkBits.push_back(ms_DefaultChunkValue);
            }

            if (m_ChunkBits[splittedIndex.ChunkIndex].test(splittedIndex.ChunkBitIndex))
            {
                break;
            }

            ++m_FreeIndex;
        }

        BenzinAssert(m_FreeIndex < m_MaxIndexCount);

        const SplittedIndex splittedIndex{ m_FreeIndex };
        m_ChunkBits[splittedIndex.ChunkIndex].set(splittedIndex.ChunkBitIndex, false);

        ++m_AllocatedIndexCount;

        return m_FreeIndex++;
    }

    void IndexAllocator::FreeIndex(uint32_t index)
    {
        BenzinAssert(index < m_MaxIndexCount);

        const SplittedIndex splittedIndex{ index };

        if (m_ChunkBits[splittedIndex.ChunkIndex].test(splittedIndex.ChunkBitIndex))
        {
            return;
        }

        m_ChunkBits[splittedIndex.ChunkIndex].set(splittedIndex.ChunkBitIndex, true);

        if (splittedIndex.ChunkIndex == (m_ChunkBits.size() - 1) && m_ChunkBits[splittedIndex.ChunkIndex].all())
        {
            m_ChunkBits.pop_back();
        }

        if (m_FreeIndex > index)
        {
            m_FreeIndex = index;
        }

        --m_AllocatedIndexCount;
    }

    void IndexAllocator::Reset()
    {
        m_ChunkBits.clear();
        m_FreeIndex = 0;
        m_AllocatedIndexCount = 0;
    }

}
