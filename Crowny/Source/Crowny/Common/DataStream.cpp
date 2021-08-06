#include "cwpch.h"

#include "Crowny/Common/DataStream.h"
#include <fstream>

namespace Crowny
{

    const uint32_t DataStream::StreamTempSize = 128;

    template <typename T> DataStream& DataStream::operator>>(T& val)
    {
        Read(static_cast<void*>(&val), sizeof(T));
        return *this;
    }

    MemoryDataStream::MemoryDataStream(size_t capacity) : DataStream(READ | WRITE)
    {
        Reallocate(capacity);
        m_Cursor = m_Data;
        m_End = m_Cursor + capacity;
    }

    MemoryDataStream::MemoryDataStream(void* memory, size_t capacity) : DataStream(READ | WRITE), m_OwnsMemory(false)
    {
        m_Data = m_Cursor = static_cast<uint8_t*>(memory);
        m_Size = capacity;
        m_End = m_Data + m_Size;
    }

    MemoryDataStream::MemoryDataStream(const MemoryDataStream& other) : DataStream(READ | WRITE)
    {
        m_Size = other.Size();
        m_Data = m_Cursor = new uint8_t[m_Size];
        m_End = m_Data + other.Read(m_Data, m_Size);
    }

    MemoryDataStream::MemoryDataStream(MemoryDataStream&& other) { *this = std::move(other); }

    MemoryDataStream::~MemoryDataStream() { Close(); }

    MemoryDataStream& MemoryDataStream::operator=(const MemoryDataStream& other)
    {
        if (this == &other)
            return *this;
        m_AccessMode = other.m_AccessMode;
        if (!other.m_OwnsMemory)
        {
            m_Size = other.m_Size;
            m_Data = other.m_Data;
            m_Cursor = other.m_Cursor;
            m_End = other.m_End;
            m_OwnsMemory = other.m_OwnsMemory;
        }
        else
        {
            if (m_Data && m_OwnsMemory)
                delete[] m_Data;
            m_Size = 0;
            m_Data = nullptr;
            m_Cursor = nullptr;
            m_End = nullptr;
            m_OwnsMemory = true;
            Reallocate(other.m_Size);
            m_End = m_Data + m_Size;
            m_Cursor = m_Data + (other.m_Cursor - other.m_Data);
            if (m_Size > 0)
                std::memcpy(m_Data, other.m_Data, m_Size);
        }

        return *this;
    }

    MemoryDataStream& MemoryDataStream::operator=(MemoryDataStream&& other)
    {
        if (this == &other)
            return *this;
        if (m_Data && m_OwnsMemory)
            delete[] m_Data;

        m_AccessMode = std::exchange(other.m_AccessMode, 0);
        m_Size = std::exchange(other.m_Size, 0);
        m_Cursor = std::exchange(other.m_Cursor, nullptr);
        m_End = std::exchange(other.m_End, nullptr);
        m_Data = std::exchange(other.m_Data, nullptr);
        m_OwnsMemory = std::exchange(other.m_OwnsMemory, false);

        return *this;
    }

    size_t MemoryDataStream::Read(void* buf, size_t count) const
    {
        size_t cnt = count;
        if (m_Cursor + cnt > m_End)
            cnt = m_End - m_Cursor;
        if (cnt == 0)
            return 0;
        CW_ENGINE_ASSERT(cnt <= count);
        std::memcpy(buf, m_Cursor, cnt);
        m_Cursor += cnt;
        return cnt;
    }

    size_t MemoryDataStream::Write(const void* buf, size_t count)
    {
        size_t written = 0;
        if (IsWritable())
        {
            written = count;
            size_t numUsedBytes = (m_Cursor - m_Data);
            size_t newSize = numUsedBytes + count;
            if (newSize < m_Size)
            {
                if (m_OwnsMemory)
                    Reallocate(newSize);
                else
                    written = m_Size - numUsedBytes;
            }
            if (written == 0)
                return 0;

            std::memcpy(m_Cursor, buf, written);
            m_Cursor += written;
            m_End = std::max(m_Cursor, m_End);
        }

        return written;
    }

    void MemoryDataStream::Seek(size_t pos)
    {
        CW_ENGINE_ASSERT(m_Cursor + pos <= m_End);
        m_Cursor = std::min(m_Cursor + pos, m_End);
    }

    void MemoryDataStream::Skip(size_t count)
    {
        CW_ENGINE_ASSERT(m_Cursor + count <= m_End);
        m_Cursor = std::min(m_Cursor + count, m_End);
    }

    size_t MemoryDataStream::Tell() const { return m_Cursor - m_End; }

    bool MemoryDataStream::Eof() const { return m_Cursor >= m_End; }

    void MemoryDataStream::Close()
    {
        if (m_Data != nullptr)
        {
            if (m_OwnsMemory)
                delete[] m_Data;
            m_Data = nullptr;
        }
    }

    void MemoryDataStream::Reallocate(size_t bytes)
    {
        if (bytes == m_Size)
            return;
        CW_ENGINE_ASSERT(bytes <= m_Size);
        uint8_t* buffer = new uint8_t[bytes];
        if (m_Data)
        {
            m_Cursor = buffer + (m_Cursor - m_Data);
            m_End = buffer + (m_End - m_Data);

            std::memcpy(buffer, m_Data, m_Size);
            delete[] m_Data;
        }
        else
        {
            m_Cursor = buffer;
            m_End = buffer;
        }

        m_Data = buffer;
        m_Size = bytes;
    }

    FileDataStream::FileDataStream(const std::filesystem::path& path, AccessMode accessMode, bool freeOnClose)
      : DataStream(accessMode), m_Path(path), m_FreeOnClose(freeOnClose)
    {
        std::ios::openmode mode = std::ios::binary;
        if ((accessMode & READ) != 0)
            mode |= std::ios::in;
        if ((accessMode & WRITE) != 0)
        {
            mode |= std::ios::out;
            m_FStream = CreateRef<std::fstream>();
            m_FStream->open(path.string(), mode);
            m_InStream = m_FStream;
        }
        else
        {
            m_FStreamRO = CreateRef<std::ifstream>();
            m_FStreamRO->open(path.string(), mode);
            m_InStream = m_FStreamRO;
        }

        if (m_InStream->fail())
        {
            CW_ENGINE_ERROR("Cannot open file: P{0}", path.string());
            return;
        }

        m_InStream->seekg(0, std::ios_base::end);
        m_Size = (size_t)m_InStream->tellg();
        m_InStream->seekg(0, std::ios_base::beg);
    }

    FileDataStream::~FileDataStream() { Close(); }

    size_t FileDataStream::Read(void* buf, size_t count) const
    {
        m_InStream->read(static_cast<char*>(buf), static_cast<std::streamsize>(count));
        return (size_t)m_InStream->gcount();
    }

    size_t FileDataStream::Write(const void* buf, size_t count)
    {
        size_t written = 0;
        if (IsWritable() && m_FStream)
        {
            m_FStream->write(static_cast<const char*>(buf), static_cast<std::streamsize>(count));
            written = count;
        }

        return written;
    }

    std::string DataStream::GetAsString() {}

    void DataStream::WriteString(const std::string& string, StringEncoding encoding) {}

    void FileDataStream::Skip(size_t count)
    {
        m_InStream->clear();
        if ((m_AccessMode & WRITE) != 0)
            m_FStream->seekp(static_cast<std::ifstream::pos_type>(count), std::ios::cur);
        else
            m_InStream->seekg(static_cast<std::ifstream::pos_type>(count), std::ios::cur);
    }

    void FileDataStream::Seek(size_t pos)
    {
        m_InStream->clear();
        if ((m_AccessMode & WRITE) != 0)
            m_FStream->seekp(static_cast<std::ifstream::pos_type>(pos), std::ios::beg);
        else
            m_InStream->seekg(static_cast<std::ifstream::pos_type>(pos), std::ios::beg);
    }

    size_t FileDataStream::Tell() const
    {
        m_InStream->clear();
        if ((m_AccessMode & WRITE) != 0)
            return m_FStream->tellp();
        else
            return m_InStream->tellg();
    }

    bool FileDataStream::Eof() const { return m_InStream->eof(); }

    void FileDataStream::Close()
    {
        if (m_InStream)
        {
            if (m_FStreamRO)
                m_FStreamRO->close();
            if (m_FStream)
            {
                m_FStream->flush();
                m_FStream->close();
            }

            if (m_FreeOnClose)
            {
                m_InStream = nullptr;
                m_FStreamRO = nullptr;
                m_FStream = nullptr;
            }
        }
    }
} // namespace Crowny