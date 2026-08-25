#pragma once

#include "Crowny/Common/StdHeaders.h"
#include "Crowny/Common/Uuid.h"

#include <glm/glm.hpp>

namespace Crowny
{
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

    struct ScriptTypeIdentity
    {
        String Assembly;
        String Namespace;
        String TypeName;

        bool IsValid() const { return !Assembly.empty() && !TypeName.empty(); }
        String GetFullName() const { return Namespace.empty() ? TypeName : Namespace + "." + TypeName; }
        bool operator==(const ScriptTypeIdentity&) const = default;
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
        String,
        Enum,
        Vector2,
        Vector3,
        Vector4,
        Quaternion,
        Matrix4,
        Entity,
        Asset,
        Array,
        List,
        Dictionary,
        Object,
        Uuid
    };

    struct ScriptValue
    {
        ScriptValueKind Kind = ScriptValueKind::Null;
        bool BooleanValue = false;
        int64_t SignedValue = 0;
        uint64_t UnsignedValue = 0;
        double FloatingValue = 0.0;
        String StringValue;
        glm::vec4 VectorValue = glm::vec4(0.0f);
        glm::mat4 MatrixValue = glm::mat4(1.0f);
        UUID ReferenceValue;
        ScriptTypeIdentity DeclaredType;
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

    enum class ScriptFieldFlags : uint32_t
    {
        None = 0,
        Serializable = 1 << 0,
        Inspectable = 1 << 1,
        ReadOnly = 1 << 2,
        Nullable = 1 << 3,
        Static = 1 << 4
    };

    constexpr ScriptFieldFlags operator|(ScriptFieldFlags lhs, ScriptFieldFlags rhs)
    {
        return static_cast<ScriptFieldFlags>(static_cast<uint32_t>(lhs) | static_cast<uint32_t>(rhs));
    }

    constexpr ScriptFieldFlags operator&(ScriptFieldFlags lhs, ScriptFieldFlags rhs)
    {
        return static_cast<ScriptFieldFlags>(static_cast<uint32_t>(lhs) & static_cast<uint32_t>(rhs));
    }

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
        Vector<String> FormerNames;
        ScriptValueKind ValueKind = ScriptValueKind::Null;
        ScriptValueKind ElementKind = ScriptValueKind::Null;
        ScriptValueKind KeyKind = ScriptValueKind::Null;
        ScriptTypeIdentity DeclaredType;
        ScriptFieldFlags Flags = ScriptFieldFlags::None;
    };

    struct ScriptTypeSchema
    {
        uint64_t StableId = 0;
        ScriptTypeIdentity Identity;
        Vector<ScriptTypeIdentity> FormerIdentities;
        ScriptTypeIdentity BaseType;
        Vector<ScriptFieldSchema> Fields;
        Vector<ScriptEventKind> Events;
    };

    struct ScriptCatalog
    {
        uint32_t ManifestVersion = 0;
        uint64_t ManifestHash = 0;
        Vector<ScriptTypeSchema> Types;

        const ScriptTypeSchema* FindType(const ScriptTypeIdentity& identity) const;
    };

    struct ScriptState
    {
        ScriptTypeIdentity Identity;
        ScriptValue Root;
        Map<String, ScriptValue> OrphanedMembers;

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

    const char* ToString(ManagedBackendId backend);
    const char* ToString(ManagedExecutionMode mode);
    ManagedOperationResult ValidateScriptCatalog(const ScriptCatalog& catalog, ManagedBackendId backend);
    ScriptStateResult MigrateScriptState(const ScriptState& state, const ScriptTypeSchema& target, ManagedBackendId backend);
} // namespace Crowny
