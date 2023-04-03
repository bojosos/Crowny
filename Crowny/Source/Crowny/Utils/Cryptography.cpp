#include "cwpch.h"

#include "Crowny/Utils/Cryptography.h"

#include <mbedtls/base64.h>
#include <mbedtls/md5.h>
#include <mbedtls/sha1.h>
#include <mbedtls/sha256.h>

namespace Crowny
{
    String Cryptography::EncodeBase64(const String& src)
    {
        return EncodeBase64((const uint8_t*)src.c_str(), src.size());
    }

    String Cryptography::EncodeBase64(const uint8_t* src, size_t length)
    {
        size_t lengthBase64 = length / 3 * 4 + 4 + 1;
        Vector<uint8_t> buff;
        buff.resize(lengthBase64);
        size_t resultLength = 0;
        int ret = mbedtls_base64_encode(buff.data(), lengthBase64, &resultLength, src, length);
        buff[resultLength] = 0;
        return ret ? String() : (const char*)buff.data();
    }

    String Cryptography::DecodeBase64(const String& src)
    {
        return DecodeBase64((const uint8_t*)src.c_str(), src.size());
    }

    String Cryptography::DecodeBase64(const uint8_t* src, size_t length)
    {
        size_t lengthDecoded = length * 3 / 4 + 1;
        Vector<uint8_t> buff;
        buff.resize(lengthDecoded);
        size_t resultLength;
        int ret = mbedtls_base64_decode(buff.data(), buff.size(), &resultLength, src, length);
        buff[resultLength] = 0;
        return ret ? String() : (const char*)buff.data();
    }

    String Cryptography::HashBytesToString(uint8_t* data, uint32_t length)
    {
        String result;
        for (uint32_t i = 0; i < 16; i++)
        {
            result += "0123456789ABCDEF"[data[i] / 16];
            result += "0123456789ABCDEF"[data[i] % 16];
        }
        return result;
    }

    int Cryptography::MD5(const uint8_t* src, size_t length, unsigned char hash[16])
    {
        return mbedtls_md5(src, length, hash);
    }

    String Cryptography::MD5(const String& src)
    {
        unsigned char hash[16];
        int ret = mbedtls_md5((const uint8_t*)src.c_str(), src.size(), hash);
        return ret ? String() : HashBytesToString(hash, 16);
    }

    int Cryptography::SHA1(const uint8_t* src, size_t length, unsigned char hash[20])
    {
        return mbedtls_sha1(src, length, hash);
    }

    String Cryptography::SHA1(const String& src)
    {
        unsigned char hash[20];
        int ret = mbedtls_sha1((const uint8_t*)src.c_str(), src.size(), hash);
        return ret ? String() : HashBytesToString(hash, 20);
    }

    int Cryptography::SHA256(const uint8_t* src, size_t length, unsigned char hash[32])
    {
        return mbedtls_sha256(src, length, hash, false);
    }

    String Cryptography::SHA256(const String& src)
    {
        unsigned char hash[32];
        int ret = mbedtls_sha256((const uint8_t*)src.c_str(), src.size(), hash, false);
        return ret ? String() : HashBytesToString(hash, 32);
    }

} // namespace Crowny