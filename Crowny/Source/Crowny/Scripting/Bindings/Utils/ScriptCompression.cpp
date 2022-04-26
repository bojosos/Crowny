#include "cwpch.h"

#include "Crowny/Scripting/Bindings/Utils/ScriptCompression.h"

#include <mono/metadata/object.h>

namespace Crowny
{
    ScriptCompression::ScriptCompression() : ScriptObject() {}

    void ScriptCompression::InitRuntimeData()
    {
        MetaData.ScriptClass->AddInternalCall("Internal_Compress", &ScriptCompression::Internal_Compress);
    }

    uint64_t ScriptCompression::Internal_Compress(MonoArray* dst, MonoArray* src, CompressionMethod method)
    {
        char* rawSrc = mono_array_addr(src, char, 0);
        char* rawDest = mono_array_addr(dst, char, 0);
        uint64_t length = mono_array_length(src);
        if (length > mono_array_length(dst))
            return -1;
        ::MonoClass* arrayClass = mono_object_get_class((MonoObject*)src);
        ::MonoClass* elementClass = mono_class_get_element_class(arrayClass);
        // TODO: Make sure element class is a char.
        CW_ENGINE_ASSERT(mono_class_array_element_size(elementClass));
        uint64_t byteSize = length * mono_class_array_element_size(elementClass); // does this code even work?
        return Compression::Compress((uint8_t*)rawSrc, (uint8_t*)rawDest, byteSize, method);
    }

} // namespace Crowny