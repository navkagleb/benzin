#pragma once

#include "benzin/graphics/fence.hpp"
#include "benzin/graphics/graphics_command_list.hpp"
#include "benzin/graphics/swap_chain.hpp"

namespace benzin
{

	class CommandQueue
	{
	public:
		BENZIN_NON_COPYABLE(CommandQueue)
		BENZIN_NON_MOVEABLE(CommandQueue)
		BENZIN_DEBUG_NAME_D3D12_OBJECT(m_D3D12CommandQueue, "CommandQueue")

	public:
		explicit CommandQueue(Device& device, std::string_view debugName);
		~CommandQueue();

	public:
		ID3D12CommandQueue* GetD3D12CommandQueue() const { return m_D3D12CommandQueue; }

		GraphicsCommandList& GetGraphicsCommandList() { BENZIN_ASSERT(m_GraphicsCommandList); return *m_GraphicsCommandList; }

	public:
		void ResetGraphicsCommandList(uint32_t commandAllocatorIndex);
		void ExecuteGraphicsCommandList();
		void Flush();

	private:
		ID3D12CommandQueue* m_D3D12CommandQueue{ nullptr };

		std::array<ID3D12CommandAllocator*, config::GetBackBufferCount()> m_D3D12CommandAllocators;
		GraphicsCommandList* m_GraphicsCommandList{ nullptr };

		Fence m_FlushFence;
		uint64_t m_FlushCount{ 0 };
	};

} // namespace benzin
