#pragma once

#include "benzin/graphics/fence.hpp"

namespace benzin
{

	class GraphicsCommandList;

	class CommandQueue
	{
	public:
		BENZIN_NON_COPYABLE(CommandQueue)
		BENZIN_NON_MOVEABLE(CommandQueue)
		BENZIN_NAME_D3D12_OBJECT(m_D3D12CommandQueue)

	public:
		explicit CommandQueue(Device& device);
		~CommandQueue();

	public:
		ID3D12CommandQueue* GetD3D12CommandQueue() const { return m_D3D12CommandQueue; }

	public:
		void Submit(GraphicsCommandList& commandList);
		void Flush();

	private:
		ID3D12CommandQueue* m_D3D12CommandQueue{ nullptr };

		Fence m_Fence;
	};

} // namespace benzin
