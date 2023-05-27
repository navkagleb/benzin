#include "benzin/config/bootstrap.hpp"

#include "benzin/graphics/api/format.hpp"

namespace benzin
{

	uint32_t GetFormatSizeInBytes(GraphicsFormat format)
	{
		switch (format)
		{
			using enum GraphicsFormat;

			case RGBA32Float: return 4 * (32 / 8);
			case RGBA16Float: return 4 * (16 / 8);
			case RGBA8Unorm: return 4 * (8 / 8);
			case RGB32Float: return 3 * (32 / 8);
			case D24Unorm_S8Uint: return 1 * (32 / 8);
			case D24Unorm_X8Typeless: return 1 * (32 / 8);
			default: break;
		}

		BENZIN_ASSERT(false);
		__assume(false);
	}

} // namespace benzin
