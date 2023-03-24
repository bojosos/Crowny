#pragma once

namespace Crowny
{
	class Cryptography
	{
    public:
        static String EncodeBase64(const String& src);
        static String EncodeBase64(const uint8_t* src, size_t length);

        static String DecodeBase64(const String& src);
        static String DecodeBase64(const uint8_t* src, size_t length);

        static String HashBytesToString(uint8_t* data, uint32_t length);

        static String MD5(const String& src);
        static int MD5(const uint8_t* src, size_t length, unsigned char hash[16]);
        static int SHA1(const uint8_t* src, size_t length, unsigned char hash[20]);
        static String SHA1(const String& src);
        static int SHA256(const uint8_t* src, size_t length, unsigned char hash[32]);
        static String SHA256(const String& src);
	};

}