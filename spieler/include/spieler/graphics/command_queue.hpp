#pragma once

#include "spieler/graphics/fence.hpp"

namespace spieler
{

	class GraphicsCommandList;

	class CommandQueue
	{
	public:
		explicit CommandQueue(Device& device);
		~CommandQueue();

	public:
		ID3D12CommandQueue* GetDX12CommandQueue() const { return m_DX12CommandQueue.Get(); }

	public:
		void Submit(GraphicsCommandList& commandList);
		void Flush();

	private:
		ComPtr<ID3D12CommandQueue> m_DX12CommandQueue;

		Fence m_Fence;
	};

} // namespace spieler
