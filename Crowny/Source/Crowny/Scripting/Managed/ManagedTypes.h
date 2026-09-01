#pragma once

#include "Crowny/Common/StdHeaders.h"
#include "Crowny/Common/Uuid.h"
#include "Crowny/Scripting/ScriptTypeIdentity.h"

#include <glm/glm.hpp>

#include <any>
#include <typeindex>

namespace Crowny
{
    inline constexpr uint32_t MANAGED_CATALOG_VERSION = 2;
    inline constexpr uint32_t MANAGED_STATE_VERSION = 1;

    enum class ManagedBackendId
    {
        Mono,
        CoreCLR,
        DotNetWasm,
        NativeAOT,
        GeneratedMetadata
    };

    enum class ManagedExecutionMode
    {
        Interpreter,
        Jit,
        ReadyToRun,
        Aot
    };

    enum class ManagedDiagnosticSeverity
    {
        Info,
        Warning,
        Error
    };

    class ScriptInstanceHandle
    {
    public:
        ScriptInstanceHandle() = default;

        bool IsValid() const { return m_Value != 0; }
        bool operator==(const ScriptInstanceHandle&) const = default;

    private:
        explicit ScriptInstanceHandle(uint64_t value) : m_Value(value) {}

        uint64_t m_Value = 0;

        friend class ManagedScripting;
    };

    enum class ScriptValueKind
    {
        Null,
        Boolean,
        SignedInteger,
        UnsignedInteger,
        Float,
        Decimal,
        String,
        Enum,
        Vector2,
        Vector3,
        Vector4,
        Color,
        Quaternion,
        Matrix4,
        Entity,
        Component,
        Asset,
        Array,
        List,
        Dictionary,
        Object,
        Uuid
    };

    enum class ScriptDictionaryLayout : uint32_t
    {
        TwoColumns,
        OneColumnWithValueFoldout,
        OneColumnWithValueVisible
    };

    struct ScriptDictionaryDisplaySettings
    {
        ScriptDictionaryLayout Layout = ScriptDictionaryLayout::TwoColumns;
        String KeyLabel = "Key";
        String ValueLabel = "Value";
        float KeyColumnFraction = 0.5f;
    };

    struct ScriptDictionaryDisplayRule
    {
        ScriptTypeIdentity TargetType;
        ScriptDictionaryDisplaySettings Display;
    };

    struct ScriptColorUsageSettings
    {
        bool ShowAlpha = true;
        bool Hdr = false;
    };

    enum class ScriptSchemaFieldFlags : uint32_t
    {
        None = 0,
        Serializable = 1 << 0,
        Inspectable = 1 << 1,
        ReadOnly = 1 << 2,
        Nullable = 1 << 3,
        Static = 1 << 4
    };

    constexpr ScriptSchemaFieldFlags operator|(ScriptSchemaFieldFlags lhs, ScriptSchemaFieldFlags rhs)
    {
        return static_cast<ScriptSchemaFieldFlags>(static_cast<uint32_t>(lhs) | static_cast<uint32_t>(rhs));
    }

    constexpr ScriptSchemaFieldFlags operator&(ScriptSchemaFieldFlags lhs, ScriptSchemaFieldFlags rhs)
    {
        return static_cast<ScriptSchemaFieldFlags>(static_cast<uint32_t>(lhs) & static_cast<uint32_t>(rhs));
    }

    enum class ScriptSearchFilterOptions : uint32_t
    {
        None = 0,
        PropertyName = 1 << 0,
        PropertyNiceName = 1 << 1,
        TypeOfValue = 1 << 2,
        ValueToString = 1 << 3,
        All = (1 << 0) | (1 << 1) | (1 << 2) | (1 << 3)
    };

    constexpr ScriptSearchFilterOptions operator|(ScriptSearchFilterOptions lhs, ScriptSearchFilterOptions rhs)
    {
        return static_cast<ScriptSearchFilterOptions>(static_cast<uint32_t>(lhs) | static_cast<uint32_t>(rhs));
    }

    constexpr ScriptSearchFilterOptions operator&(ScriptSearchFilterOptions lhs, ScriptSearchFilterOptions rhs)
    {
        return static_cast<ScriptSearchFilterOptions>(static_cast<uint32_t>(lhs) & static_cast<uint32_t>(rhs));
    }

    struct ScriptSearchSettings
    {
        ScriptSearchFilterOptions FilterOptions = ScriptSearchFilterOptions::All;
        bool FuzzySearch = true;
        bool Recursive = true;
    };

    enum class ScriptProgressBarLabelAlignment : uint32_t
    {
        Left,
        Center,
        Right
    };

    struct ScriptProgressBarSettings
    {
        double Min = 0.0;
        double Max = 0.0;
        String MinGetter;
        String MaxGetter;
        glm::vec3 Color = glm::vec3(0.15f, 0.47f, 0.74f);
        uint32_t Height = 12;
        bool Segmented = false;
        bool DrawValueLabel = true;
        ScriptProgressBarLabelAlignment ValueLabelAlignment = ScriptProgressBarLabelAlignment::Center;
        String ColorGetter;
        String BackgroundColorGetter;
        String CustomValueStringGetter;
    };

    enum class ScriptPathKind : uint32_t
    {
        File,
        Folder
    };

    struct ScriptPathSettings
    {
        ScriptPathKind Kind = ScriptPathKind::File;
        bool AbsolutePath = false;
        String ParentFolder;
        bool RequireExistingPath = false;
        bool UseBackslashes = false;
        String Extensions;
        bool IncludeFileExtension = true;
    };

    struct ScriptMultilineSettings
    {
        int32_t Lines = 3;
    };

    struct ScriptEnumOption
    {
        String Name;
        uint64_t Value = 0;
    };

    struct ScriptEnumButtonsSettings
    {
        bool IsFlags = false;
        bool IsUnsigned = false;
        bool IncludeObsolete = false;
        Vector<ScriptEnumOption> Options;
    };

    struct ScriptTooltipSettings
    {
        String Text;
    };

    enum class ScriptButtonStyle : uint32_t
    {
        Box,
        CompactBox,
        FoldoutButton
    };

    enum class ScriptButtonIconAlignment : uint32_t
    {
        Left,
        Right
    };

    struct ScriptButtonSettings
    {
        String Name;
        int32_t ButtonHeight = 0;
        float ButtonAlignment = 0.5f;
        bool Stretch = true;
        ScriptButtonStyle Style = ScriptButtonStyle::CompactBox;
        bool DisplayParameters = true;
        bool Expanded = false;
        bool DrawResult = true;
        bool DirtyOnClick = true;
        String Icon;
        ScriptButtonIconAlignment IconAlignment = ScriptButtonIconAlignment::Left;
    };

    enum class ScriptConditionEffect : uint32_t
    {
        Show,
        Hide,
        Enable,
        Disable
    };

    struct ScriptLabelSettings
    {
        String Text;
    };

    class ScriptInspectorAttributeSet
    {
    public:
        // Attribute metadata is decoded once and retained as its native settings type for inspector draws.
        template <typename T> void Set(T value) { m_Attributes.insert_or_assign(std::type_index(typeid(T)), std::move(value)); }

        template <typename T> const T* Get() const
        {
            const auto attribute = m_Attributes.find(std::type_index(typeid(T)));
            return attribute != m_Attributes.end() ? std::any_cast<T>(&attribute->second) : nullptr;
        }

        template <typename T> bool Has() const { return Get<T>() != nullptr; }
        bool Empty() const { return m_Attributes.empty(); }

    private:
        UnorderedMap<std::type_index, std::any> m_Attributes;
    };

    struct ScriptValue
    {
        ScriptValueKind Kind = ScriptValueKind::Null;
        bool BooleanValue = false;
        int64_t SignedValue = 0;
        uint64_t UnsignedValue = 0;
        double FloatingValue = 0.0;
        bool EnumUnsigned = false;
        String StringValue;
        glm::vec4 VectorValue = glm::vec4(0.0f);
        glm::mat4 MatrixValue = glm::mat4(1.0f);
        UUID ReferenceValue;
        ScriptTypeIdentity DeclaredType;
        ScriptInspectorAttributeSet Attributes;
        Vector<ScriptValue> Elements;
        Map<String, ScriptValue> Members;

        static ScriptValue Null();
        static ScriptValue Boolean(bool value);
        static ScriptValue Signed(int64_t value);
        static ScriptValue Unsigned(uint64_t value);
        static ScriptValue Float(double value);
        static ScriptValue Text(String value);
        static ScriptValue Object(Map<String, ScriptValue> members, ScriptTypeIdentity type = {});

        bool operator==(const ScriptValue& other) const;
    };

    struct ScriptConditionRule
    {
        ScriptConditionEffect Effect = ScriptConditionEffect::Show;
        String Condition;
        bool Animate = true;
        bool HasValue = false;
        ScriptValue Value;
        bool HasResolvedResult = false;
        bool ResolvedResult = false;
    };

    struct ScriptConditionalSettings
    {
        Vector<ScriptConditionRule> Rules;
    };

    struct ScriptValueChangedAction
    {
        String Action;
        uint64_t MethodId = 0;
        bool IncludeChildren = false;
        bool InvokeOnInitialize = false;
        bool InvokeOnUndoRedo = true;
        bool PassValue = false;
    };

    struct ScriptOnValueChangedSettings
    {
        Vector<ScriptValueChangedAction> Actions;
    };

    enum class ScriptEventKind
    {
        Start,
        Update,
        Destroy,
        CollisionEnter2D,
        CollisionStay2D,
        CollisionExit2D,
        TriggerEnter2D,
        TriggerStay2D,
        TriggerExit2D,
        CollisionEnter3D,
        CollisionStay3D,
        CollisionExit3D,
        TriggerEnter3D,
        TriggerStay3D,
        TriggerExit3D
    };

    struct ScriptContactPoint
    {
        glm::vec3 Position = glm::vec3(0.0f);
        glm::vec3 Normal = glm::vec3(0.0f);
        float Separation = 0.0f;
        float Impulse = 0.0f;
    };

    struct ScriptEvent
    {
        ScriptEventKind Kind = ScriptEventKind::Update;
        float DeltaTime = 0.0f;
        UUID OtherEntity;
        glm::vec3 RelativeVelocity = glm::vec3(0.0f);
        Vector<ScriptContactPoint> Contacts;

        static ScriptEvent Lifecycle(ScriptEventKind kind, float deltaTime = 0.0f);
    };

    struct ScriptFieldSchema
    {
        uint64_t StableId = 0;
        String Name;
        ScriptValueKind ValueKind = ScriptValueKind::Null;
        ScriptValueKind ElementKind = ScriptValueKind::Null;
        ScriptValueKind KeyKind = ScriptValueKind::Null;
        ScriptTypeIdentity DeclaredType;
        ScriptSchemaFieldFlags Flags = ScriptSchemaFieldFlags::None;
        ScriptInspectorAttributeSet Attributes;
    };

    struct ScriptMethodParameterSchema
    {
        String Name;
        ScriptValueKind ValueKind = ScriptValueKind::Null;
        ScriptTypeIdentity DeclaredType;
        bool HasDefaultValue = false;
        ScriptValue DefaultValue;
    };

    struct ScriptMethodSchema
    {
        uint64_t StableId = 0;
        String Name;
        bool IsStatic = false;
        ScriptValueKind ReturnKind = ScriptValueKind::Null;
        ScriptTypeIdentity DeclaredReturnType;
        Vector<ScriptMethodParameterSchema> Parameters;
        ScriptInspectorAttributeSet Attributes;
    };

    enum class ScriptTypeFlags : uint32_t
    {
        None = 0,
        RunInEditor = 1 << 0
    };

    constexpr ScriptTypeFlags operator|(ScriptTypeFlags lhs, ScriptTypeFlags rhs)
    {
        return static_cast<ScriptTypeFlags>(static_cast<uint32_t>(lhs) | static_cast<uint32_t>(rhs));
    }

    constexpr ScriptTypeFlags operator&(ScriptTypeFlags lhs, ScriptTypeFlags rhs)
    {
        return static_cast<ScriptTypeFlags>(static_cast<uint32_t>(lhs) & static_cast<uint32_t>(rhs));
    }

    struct ScriptTypeSchema
    {
        uint64_t StableId = 0;
        ScriptTypeIdentity Identity;
        ScriptTypeIdentity BaseType;
        Vector<ScriptFieldSchema> Fields;
        Vector<ScriptMethodSchema> Methods;
        Vector<ScriptEventKind> Events;
        ScriptTypeFlags Flags = ScriptTypeFlags::None;
        ScriptInspectorAttributeSet Attributes;
    };

    struct ScriptCatalog
    {
        uint32_t ManifestVersion = 0;
        uint64_t ManifestHash = 0;
        Vector<ScriptTypeSchema> Types;
        Vector<ScriptDictionaryDisplayRule> DictionaryDisplays;

        const ScriptTypeSchema* FindType(const ScriptTypeIdentity& identity) const;
        const ScriptDictionaryDisplaySettings* FindDictionaryDisplay(const ScriptTypeIdentity& identity) const;
    };

    struct ScriptState
    {
        ScriptTypeIdentity Identity;
        ScriptValue Root;

        bool operator==(const ScriptState&) const = default;
    };

    struct ManagedCapabilities
    {
        bool DynamicProgramLoading = false;
        bool Reload = false;
        bool RuntimeReflection = false;
        bool ManagedDebugging = false;
        bool Profiling = false;
        bool Threads = false;
        bool NativeDynamicLibraries = false;
        bool AotOnly = false;
        bool WebAssembly = false;
    };

    struct ManagedDiagnostic
    {
        ManagedDiagnosticSeverity Severity = ManagedDiagnosticSeverity::Error;
        String Code;
        String Message;
        String ManagedStack;
        ManagedBackendId Backend = ManagedBackendId::Mono;
        ScriptTypeIdentity Script;
        UUID Entity;
    };

    struct ManagedOperationResult
    {
        bool Succeeded = true;
        Vector<ManagedDiagnostic> Diagnostics;

        static ManagedOperationResult Success();
        static ManagedOperationResult Failure(String code, String message, ManagedBackendId backend);
        bool HasDiagnosticCode(StringView code) const;
    };

    enum class ManagedProgramArtifactKind
    {
        EngineAssembly,
        GameAssembly,
        DependencyAssembly,
        Symbols,
        RuntimeConfig,
        DependencyManifest,
        GeneratedMetadata,
        NativeLibrary
    };

    struct ManagedProgramArtifact
    {
        ManagedProgramArtifactKind Kind = ManagedProgramArtifactKind::GameAssembly;
        String LogicalName;
        Path Filepath;
    };

    struct ManagedProgramDefinition
    {
        uint64_t Generation = 0;
        Vector<ManagedProgramArtifact> Artifacts;
        ScriptCatalog Catalog;
    };

    struct ManagedScriptingConfig
    {
        ManagedBackendId Backend = ManagedBackendId::Mono;
        ManagedExecutionMode ExecutionMode = ManagedExecutionMode::Jit;
        Path RuntimeRoot;
        bool EnableDebugging = false;
        bool EnableProfiling = false;
    };

    struct ScriptCreateRequest
    {
        ScriptTypeIdentity Identity;
        UUID Entity;
        uint64_t RuntimeInstanceId = 0;
        ScriptState InitialState;
    };

    struct ScriptCreateResult
    {
        ManagedOperationResult Result;
        ScriptInstanceHandle Handle;
    };

    struct ScriptStateResult
    {
        ManagedOperationResult Result;
        ScriptState State;
    };

    struct ScriptInvocationResult
    {
        ManagedOperationResult Result;
        bool HasReturnValue = false;
        ScriptValue ReturnValue;
    };

    const char* ToString(ManagedBackendId backend);
    const char* ToString(ManagedExecutionMode mode);
    ManagedOperationResult ValidateScriptCatalog(const ScriptCatalog& catalog, ManagedBackendId backend);
    ScriptStateResult NormalizeScriptState(const ScriptState& state, const ScriptTypeSchema& target, ManagedBackendId backend);
} // namespace Crowny
