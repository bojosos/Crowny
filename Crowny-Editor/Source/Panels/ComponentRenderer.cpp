#include "cwepch.h"

#include "Panels/ComponentEditor.h"

#include "Crowny/Ecs/Components.h"
#include "UI/Properties.h"
#include "UI/ScriptInspector.h"

#include <imgui.h>

namespace Crowny
{
    template <> void ComponentEditorWidget<MonoScriptComponent>(Entity entity)
    {
        MonoScriptComponent& scriptComponent = entity.GetComponent<MonoScriptComponent>();
        for (uint32_t i = 0; i < scriptComponent.Scripts.size(); i++)
        {
            auto& script = scriptComponent.Scripts[i];
            ImGui::PushID(i);
            if (ImGui::Button("-"))
            {
                const ScriptTypeIdentity identity = scriptComponent.Scripts[i].GetTypeIdentity();
                entity.GetScene()->RemoveScriptComponent(entity, identity);
                UndoRedo::Get().OnItemInteract(true);
                ImGui::PopID();
                if (!entity.HasComponent<MonoScriptComponent>())
                    return;
                continue;
            }
            ImGui::SameLine();
            ImGui::SetNextItemOpen(true, ImGuiCond_Once);
            if (ImGui::CollapsingHeader(script.GetTypeName().c_str()))
            {
                ImGui::Indent(30.f);
                ImGui::PushID("Widget");
                UI::BeginPropertyGrid();

                String typeName = script.GetTypeName();
                if (UIUtils::PropertyScript("Class name", typeName))
                {
                    script.SetClassName(typeName);
                    script.Create(entity);
                }

                if (script.GetManagedClass() == nullptr)
                {
                    UI::EndPropertyGrid();
                    ImGui::PopID();
                    ImGui::Unindent(30.f);
                    ImGui::PopID();
                    continue;
                }

                Ref<SerializableObjectInfo> objectInfo = script.GetObjectInfo();
                MonoObject* instance = script.GetManagedInstance();
                ScriptInspector::DrawObjectInspector(objectInfo, instance, nullptr);

                UI::EndPropertyGrid();
                ImGui::PopID();
                ImGui::Unindent(30.f);
            }
            ImGui::PopID();
        }
    }
} // namespace Crowny
