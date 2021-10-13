#pragma once

#include "Crowny/Common/DataStream.h"

#include "Crowny/Assets/CerealDataStreamArchive.h"

#include "Crowny/Assets/SerializeUtils.h"

#include <cereal/cereal.hpp>
#include <cereal/types/base_class.hpp>
#include <cereal/types/polymorphic.hpp>
#include <cereal/types/unordered_map.hpp>
#include <cereal/types/vector.hpp>

using namespace Crowny;
using namespace cereal;

class BinaryDataStreamOutputArchive : public OutputArchive<BinaryDataStreamOutputArchive, AllowEmptyClassElision>
{
public:
    BinaryDataStreamOutputArchive(const Ref<DataStream>& stream)
      : OutputArchive<BinaryDataStreamOutputArchive, AllowEmptyClassElision>(this), m_Stream(stream)
    {
    }

    void saveBinary(const void* data, std::streamsize size)
    {
        auto const writtenSize = m_Stream->Write(data, size);
        CW_ENGINE_ASSERT(writtenSize == size);
    }

    Ref<DataStream> GetStream() { return m_Stream; };

private:
    Ref<DataStream> m_Stream;
};

class BinaryDataStreamInputArchive : public InputArchive<BinaryDataStreamInputArchive, AllowEmptyClassElision>
{
public:
    BinaryDataStreamInputArchive(const Ref<DataStream>& stream)
      : InputArchive<BinaryDataStreamInputArchive, AllowEmptyClassElision>(this), m_Stream(stream)
    {
    }

    void loadBinary(void* const data, std::streamsize size)
    {
        auto const readSize = m_Stream->Read(data, size);
        CW_ENGINE_ASSERT(readSize == size);
    }

    Ref<DataStream> GetStream() { return m_Stream; };

private:
    Ref<DataStream> m_Stream;
};

template <class T>
inline typename std::enable_if<std::is_arithmetic<T>::value, void>::type CEREAL_SAVE_FUNCTION_NAME(
  BinaryDataStreamOutputArchive& ar, T const& t)
{
    ar.saveBinary(std::addressof(t), sizeof(t));
}

//! Loading for POD types from binary
template <class T>
inline typename std::enable_if<std::is_arithmetic<T>::value, void>::type CEREAL_LOAD_FUNCTION_NAME(
  BinaryDataStreamInputArchive& ar, T& t)
{
    ar.loadBinary(std::addressof(t), sizeof(t));
}

//! Serializing NVP types to binary
template <class Archive, class T>
inline CEREAL_ARCHIVE_RESTRICT(BinaryDataStreamInputArchive, BinaryDataStreamOutputArchive)
  CEREAL_SERIALIZE_FUNCTION_NAME(Archive& ar, NameValuePair<T>& t)
{
    ar(t.value);
}

//! Serializing SizeTags to binary
template <class Archive, class T>
inline CEREAL_ARCHIVE_RESTRICT(BinaryDataStreamInputArchive, BinaryDataStreamOutputArchive)
  CEREAL_SERIALIZE_FUNCTION_NAME(Archive& ar, SizeTag<T>& t)
{
    ar(t.size);
}

//! Saving binary data
template <class T> inline void CEREAL_SAVE_FUNCTION_NAME(BinaryDataStreamOutputArchive& ar, BinaryData<T> const& bd)
{
    ar.saveBinary(bd.data, static_cast<std::streamsize>(bd.size));
}

//! Loading binary data
template <class T> inline void CEREAL_LOAD_FUNCTION_NAME(BinaryDataStreamInputArchive& ar, BinaryData<T>& bd)
{
    ar.loadBinary(bd.data, static_cast<std::streamsize>(bd.size));
}

CEREAL_REGISTER_ARCHIVE(BinaryDataStreamOutputArchive)
CEREAL_REGISTER_ARCHIVE(BinaryDataStreamInputArchive)

CEREAL_SETUP_ARCHIVE_TRAITS(BinaryDataStreamInputArchive, BinaryDataStreamOutputArchive)
