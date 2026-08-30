#include "cwpch.h"

#include "Crowny/Scripting/Backends/Mono/MonoObjectIdentity.h"

#include "Crowny/Scripting/Mono/MonoClass.h"
#include "Crowny/Scripting/Mono/MonoField.h"
#include "Crowny/Scripting/Mono/MonoManager.h"
#include "Crowny/Scripting/Mono/MonoUtils.h"

namespace Crowny::MonoObjectIdentity
{
    namespace
    {
        bool SetUuid(MonoObject* instance, StringView fieldName, const UUID& value)
        {
            if (instance == nullptr || !MonoManager::IsStartedUp())
                return false;
            MonoClass* managedClass = MonoManager::Get().FindClass(MonoUtils::GetClass(instance));
            MonoField* field = managedClass != nullptr ? managedClass->GetField(fieldName) : nullptr;
            if (field == nullptr)
                return false;
            UUID copy = value;
            field->Set(instance, &copy);
            return true;
        }
    } // namespace

    bool SetEntity(MonoObject* instance, const UUID& value) { return SetUuid(instance, "m_ManagedUuid", value); }

    bool SetComponentEntity(MonoObject* instance, const UUID& value) { return SetUuid(instance, "m_ManagedEntityId", value); }

    bool SetAsset(MonoObject* instance, const UUID& value) { return SetUuid(instance, "m_ManagedUuid", value); }
} // namespace Crowny::MonoObjectIdentity
