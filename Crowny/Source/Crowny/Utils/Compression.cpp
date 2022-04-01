#include "cwpch.h"

#include "Crowny/Utils/Compression.h"
#include "Vendor/fastlz/fastlz.h"

namespace Crowny
{
	uint64_t Compression::Compress(uint8_t* dest, const uint8_t* src, uint64_t size, CompressionMethod method)
	{
		switch (method)
		{
		case CompressionMethod::FastLZ:
		{
			if (size < 32)
			{
				std::memcpy(dest, src, size);
				return size;
			}
			return fastlz_compress(src, size, dest);
		}			
		default:
			CW_ENGINE_ERROR("Unsupported compression method.");
		}
	}
}