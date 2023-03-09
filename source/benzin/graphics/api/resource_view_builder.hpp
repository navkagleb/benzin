#pragma once

#include "benzin/graphics/api/texture.hpp"

namespace benzin
{

	struct BufferShaderResourceViewConfig
	{
		uint32_t BeginElement{ 0 };
		uint32_t ElementCount{ 0 };
	};

	struct TextureShaderResourceViewConfig
	{
		TextureResource::Type Type{ TextureResource::Type::Unknown };
		GraphicsFormat Format{ GraphicsFormat::Unknown };
		bool IsCubeMap{ false };
		uint32_t MostDetailedMipIndex{ 0 };
		uint32_t MipCount{ 0xffffffff }; // Default value to select all mips
		uint32_t FirstArrayIndex{ 0 };
		uint32_t ArraySize{ 0 };
	};

	struct TextureRenderTargetViewConfig
	{
		GraphicsFormat Format{ GraphicsFormat::Unknown };
		uint32_t FirstArrayIndex{ 0 };
		uint32_t ArraySize{ 0 };
	};

	class ResourceViewBuilder
	{
	public:
		explicit ResourceViewBuilder(Device& device);

	public:
		Descriptor CreateConstantBufferView(const BufferResource& bufferResource);

		Descriptor CreateShaderResourceView(const BufferResource& bufferResource);
		Descriptor CreateShaderResourceView(const TextureResource& textureResource, const TextureShaderResourceViewConfig& config = {});
		Descriptor CreateUnorderedAccessView(const TextureResource& textureResource);

		Descriptor CreateRenderTargetView(const TextureResource& textureResource, const TextureRenderTargetViewConfig& config = {});
		Descriptor CreateDepthStencilView(const TextureResource& textureResource);

		void InitShaderResourceViewForDescriptor(const Descriptor& descriptor, const TextureResource& textureResource, const TextureShaderResourceViewConfig& config = {}) const;
		void InitUnorderedAccessViewForDescriptor(const Descriptor& descriptor, const TextureResource& textureResource) const;

		void InitRenderTargetViewForDescriptor(const Descriptor& descriptor, const TextureResource& textureResource, const TextureRenderTargetViewConfig& config = {}) const;
		void InitDepthStencilViewForDescriptor(const Descriptor& descriptor, const TextureResource& textureResource) const;

	private:
		Device& m_Device;
	};

} // namespace benzin
