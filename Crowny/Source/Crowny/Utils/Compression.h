#pragma once

namespace Crowny
{
	
	enum class CompressionMethod
	{
		// Really fast compression, however, compression ratio will be slightly lower
		FastLZ,
		Deflate,
		Zstd
	};
	
	class Compression
	{
	public:
		static uint64_t Compress(uint8_t* dest, const uint8_t* src, uint64_t size, CompressionMethod method);
		
	private:
	};
}
