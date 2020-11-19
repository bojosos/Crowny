#pragma once

#include "Crowny.h"

#include <entt/entt.hpp>
#include <imgui.h>

namespace Crowny {

	template <class Component>
	void ComponentEditorWidget(Entity& entity) 
	{ 
	
	}

	template <class Component>
	void ComponentAddAction(Entity& entity)
	{
		entity.AddComponent<Component>();
		entity.GetComponent<Component>().ComponentParent = entity;
	}

	template <class Component>
	void ComponentRemoveAction(Entity& entity)
	{
		entity.RemoveComponent<Component>();
	}

	class ImGuiComponentEditor {
	public:
		struct ComponentInfo {
			using Callback = std::function<void(Entity&)>;
			std::string name;
			Callback widget, create, destroy;
		};

		bool show_window = true;

	private:
		using ComponentTypeID = ENTT_ID_TYPE;

		std::map<ComponentTypeID, ComponentInfo> component_infos;

		bool EntityHasComponent(const entt::registry& registry, entt::entity& entity, ComponentTypeID type_id)
		{
			ComponentTypeID type[] = { type_id };
			return registry.runtime_view(std::cbegin(type), std::cend(type)).contains(entity);
		}

	public:
		template <class Component>
		ComponentInfo& RegisterComponent(const ComponentInfo& component_info)
		{
			auto index = entt::type_info<Component>::id();
			auto [it, insert_result] = component_infos.insert_or_assign(index, component_info);
			assert(insert_result);
			return std::get<ComponentInfo>(*it);
		}

		template <class Component>
		ComponentInfo& RegisterComponent(const std::string& name, typename ComponentInfo::Callback widget)
		{
			return RegisterComponent<Component>(ComponentInfo{
				name,
				widget,
				ComponentAddAction<Component>,
				ComponentRemoveAction<Component>,
			});
		}

		template <class Component>
		ComponentInfo& RegisterComponent(const std::string& name)
		{
			return RegisterComponent<Component>(name, ComponentEditorWidget<Component>);
		}

		void Render(Entity entity)
		{
			entt::registry& registry = SceneManager::GetActiveScene()->m_Registry;
			entt::entity e = entity.m_EntityHandle;

			ImGui::Separator();

			if (e != entt::null) {
				ImGui::Text(entity.GetComponent<TagComponent>().Tag.c_str());
			}
			else {
				ImGui::Text("Invalid Entity");
			}

			ImGui::Separator();

			if (registry.valid(e)) {
				ImGui::PushID(entt::to_integral(e));
				std::map<ComponentTypeID, ComponentInfo> has_not;
				for (auto& [component_type_id, ci] : component_infos) {
					if (EntityHasComponent(registry, e, component_type_id)) {
						ImGui::PushID(component_type_id);
						if (ImGui::Button("-")) {
							ci.destroy(entity);
							ImGui::PopID();
							continue; // early out to prevent access to deleted data
						}
						else {
							ImGui::SameLine();
						}
						ImGui::SetNextItemOpen(true, ImGuiCond_Once);
						if (ImGui::CollapsingHeader(ci.name.c_str())) {
							ImGui::Indent(30.f);
							ImGui::PushID("Widget");
							ci.widget(entity);
							ImGui::PopID();
							ImGui::Unindent(30.f);
						}
						ImGui::PopID();
					}
					else {
						has_not[component_type_id] = ci;
					}
				}

				if (!has_not.empty()) {
					if (ImGui::Button("+ Add Component")) {
						ImGui::OpenPopup("Add Component");
					}

					ImGui::SetNextItemWidth(200);
					if (ImGui::BeginPopup("Add Component")) {
						ImGui::Text("Components:  ");
						ImGui::Separator();

						for (auto& [component_type_id, ci] : has_not) {
							ImGui::PushID(component_type_id);
							if (ImGui::Selectable(ci.name.c_str())) {
								ci.create(entity);
							}
							ImGui::PopID();
						}
						ImGui::EndPopup();
					}
				}
				ImGui::PopID();
			}
		}
	};

}

// MIT License

// Copyright (c) 2020 Erik Scholz, Gnik Droy

// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.