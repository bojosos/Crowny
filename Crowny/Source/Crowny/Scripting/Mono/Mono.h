#pragma once

#define CROWNY_NS "Crowny"
#define CROWNY_ASSEMBLY "CrownySharp"
#define GAME_ASSEMBLY "GameAssembly"

namespace Crowny
{
#undef Bool
    enum class MonoPrimitiveType
    {
        Bool,
        Char,
        I8,
        U8,
        I16,
        U16,
        I32,
        U32,
        I64,
        U64,
        String,
        Float,
        Double,
        ValueType,
        Class,
        Array,
        Generic,
        Enum,
        Unknown
    };

} // namespace Crowny

typedef struct _MonoClass MonoClass;
typedef struct _MonoDomain MonoDomain;
typedef struct _MonoImage MonoImage;
typedef struct _MonoAssembly MonoAssembly;
typedef struct _MonoMethod MonoMethod;
typedef struct _MonoProperty MonoProperty;
typedef struct _MonoObject MonoObject;
typedef struct _MonoString MonoString;
typedef struct _MonoArray MonoArray;
typedef struct _MonoReflectionType MonoReflectionType;
typedef struct _MonoException MonoException;
typedef struct _MonoClassField MonoClassField;