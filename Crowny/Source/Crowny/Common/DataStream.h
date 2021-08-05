#pragma once

namespace Crowny
{

    enum class StringEncoding
    {
        UTF8,
        UTF16
    };
    
    class DataStream
    {
    public:
        enum AccessMode
        {
            READ = 1,
            WRITE = 2
        };
    public:
        DataStream(uint16_t accessMode = READ) : m_AccessMode(accessMode)
        { }

        virtual ~DataStream() = default;
        uint16_t GetAccessMode() const { return m_AccessMode; }
        virtual bool IsReadable() const { return (m_AccessMode & READ) != 0; }
        virtual bool IsWritable() const { return (m_AccessMode & WRITE) != 0; }
        
        template<typename T>
        DataStream& operator>>(T& val);
        virtual bool IsFile() const = 0;
        virtual size_t Read(void* buf, size_t count) const = 0;
        virtual size_t Write(const void* buf, size_t count) { return 0; }
        virtual void WriteString(const std::string& string, StringEncoding encoding = StringEncoding::UTF8);
        virtual std::string GetAsString();
        
        virtual void Skip(size_t count) = 0;
        virtual void Seek(size_t pos) = 0;
        virtual size_t Tell() const = 0;
        size_t Size() const { return m_Size; }
        virtual void Close() = 0;
        virtual bool Eof() const = 0;

    protected:
        static const uint32_t StreamTempSize;
        size_t m_Size = 0;
        uint16_t m_AccessMode;
        
    };
    
    class MemoryDataStream : public DataStream
    {
    public:
        MemoryDataStream();
        MemoryDataStream(size_t capacity);
        MemoryDataStream(void* memory, size_t capacity);
        MemoryDataStream(const MemoryDataStream& other);
        MemoryDataStream(const Ref<MemoryDataStream>& other);
        MemoryDataStream(MemoryDataStream&& other);
        ~MemoryDataStream();
        
        MemoryDataStream& operator=(const MemoryDataStream& other);
        MemoryDataStream& operator=(MemoryDataStream&& other);
        
        virtual bool IsFile() const override { return false; }
        virtual size_t Read(void* buf, size_t count) const override;
        virtual size_t Write(const void* buf, size_t count) override;
        
        virtual void Skip(size_t count) override;
        virtual void Seek(size_t pos) override;
        virtual size_t Tell() const override;
        uint8_t* Data() const { return m_Data; }
        uint8_t* Cursor() const { return m_Cursor; }
        virtual void Close() override;
        virtual bool Eof() const override;
    private:
        void Reallocate(size_t bytes);
        uint8_t* m_Data = nullptr;
        mutable uint8_t* m_Cursor = nullptr;
        uint8_t* m_End = nullptr;
        bool m_OwnsMemory = true;
    };
    
    class FileDataStream : public DataStream
    {
    public:
        FileDataStream(const std::filesystem::path& path, AccessMode accessMode = READ, bool freeOnClose = true);
        ~FileDataStream();
        bool IsFile() const override { return true; }
        virtual size_t Read(void* buf, size_t count) const override;
        virtual size_t Write(const void* buf, size_t count) override;
        
        virtual void Skip(size_t count) override;
        virtual void Seek(size_t pos) override;
        virtual size_t Tell() const override;
        virtual bool Eof() const override;
        
        virtual void Close() override;
        
    private:
        std::filesystem::path m_Path;
        Ref<std::istream> m_InStream;
        Ref<std::ifstream> m_FStreamRO;
        Ref<std::fstream> m_FStream;
        bool m_FreeOnClose;
    };

}