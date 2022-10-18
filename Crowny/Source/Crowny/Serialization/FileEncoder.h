#pragma once

#include "Crowny/Serialization/AssetManifestSerializer.h"
#include "Crowny/Serialization/CerealDataStreamArchive.h"

namespace Crowny
{

    enum class SerializerType
    {
        // Serialize in a text(yaml) format. This is used for editor structures.
        Yaml,
        // Serialize to binary. This is used in a packaged game.
        Binary,
        // Determine automatically if binary or yaml should be used. If running Crowny-Editor->Text, otherwise binary.
        Auto
    };

    template <typename T, SerializerType S = SerializerType::Auto> class FileEncoder;

    template <typename T> class FileEncoder<T, SerializerType::Binary>
    {
    public:
        FileEncoder(const Path& path) : m_OutputPath(path) {}

        void Encode(const Ref<T>& serializable)
        {
            Ref<DataStream> stream = FileSystem::CreateAndOpenFile(m_OutputPath);
            BinaryDataStreamOutputArchive archive(stream);
            archive(serializable);
            stream->Close();
        }

    private:
        Path m_OutputPath;
    };

    template <typename T> class FileEncoder<T, SerializerType::Yaml>
    {
    public:
        FileEncoder(const Path& path) : m_OutputPath(path) {}

        void Encode(const Ref<T>& serializable)
        {
            YAML::Emitter out;
            T::Serializer::Serialize(serializable, out);
            Ref<DataStream> stream = FileSystem::CreateAndOpenFile(m_OutputPath);
            const char* str = out.c_str();
            stream->Write(str, std::strlen(str));
            stream->Close();
        }

    private:
        Path m_OutputPath;
    };

    template <typename T> class FileEncoder<T, SerializerType::Auto>
    {
    public:
        FileEncoder(const Path& path) : m_OutputPath(path) {}

        void Encode(const Ref<T>& serializable)
        {
#ifdef CW_EDITOR
            YAML::Emitter out;
            T::Serializer::Serialize(serializable, out);
            Ref<DataStream> stream = FileSystem::CreateAndOpenFile(m_OutputPath);
            const char* str = out.c_str();
            stream->Write(str, std::strlen(str));
            stream->Close();
#else
            Ref<DataStream> stream = FileSystem::CreateAndOpenFile(m_OutputPath);
            BinaryDataStreamOutputArchive archive(stream);
            archive(serializable);
            stream->Close();
#endif
        }

    private:
        Path m_OutputPath;
    };

    template <typename T, SerializerType S = SerializerType::Auto> class FileDecoder;

    template <typename T> class FileDecoder<T, SerializerType::Binary>
    {
    public:
        FileDecoder(const Path& path) : m_InputPath(path) {}

        Ref<T> Decode()
        {
            Ref<DataStream> stream = FileSystem::OpenFile(m_InputPath);
            BinaryDataStreamInputArchive archive(stream);
            Ref<T> result;
            archive(result);
            stream->Close();
            return result;
        }

    private:
        Path m_InputPath;
    };

    template <typename T> class FileDecoder<T, SerializerType::Yaml>
    {
    public:
        FileDecoder(const Path& path) : m_InputPath(path) {}

        Ref<T> Decode()
        {
            String text = FileSystem::OpenFile(m_InputPath, true)->GetAsString();
            YAML::Node data = YAML::Load(text);
            return T::Serializer::Deserialize(data);
        }

    private:
        Path m_InputPath;
    };

    template <typename T> class FileDecoder<T, SerializerType::Auto>
    {
    public:
        FileDecoder(const Path& path) : m_OutputPath(path) {}

        Ref<T> Decode()
        {
#ifdef CW_EDITOR
            String text = FileSystem::OpenFile(m_InputPath)->GetAsString();
            YAML::Node data = YAML::Load(text);
            return T::Serializer::Deserialize(data);
#else
            Ref<DataStream> stream = FileSystem::OpenFile(m_InputPath, true);
            BinaryDataStreamOutputArchive archive(stream);
            Ref<T> result;
            archive(result);
            stream->Close();
            return result;
#endif
        }

    private:
        Path m_InputPath;
    };
} // namespace Crowny
