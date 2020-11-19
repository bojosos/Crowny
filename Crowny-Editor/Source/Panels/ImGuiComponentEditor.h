#pragma once

#include "Crowny.h"
#include "Panels/ImGuiPanel.h"

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

	class ImGuiComponentEditor
	{
	public:
		struct ComponentInfo {
			using Callback = std::function<void(Entity&)>;
			std::string name;
			Callback widget, create, destroy;
		};

	private:
		using ComponentTypeID = ENTT_ID_TYPE;

		std::map<ComponentTypeID, ComponentInfo> m_ComponentInfos;

		bool EntityHasComponent(const entt::registry& registry, entt::entity& entity, ComponentTypeID tid)
		{
			ComponentTypeID type[] = { tid };
			return registry.runtime_view(std::cbegin(type), std::cend(type)).contains(entity);
		}

	public:
		template <class Component>
		ComponentInfo& RegisterComponent(const ComponentInfo& componentInfo)
		{
			auto index = entt::type_info<Component>::id();
			auto [it, res] = m_ComponentInfos.insert_or_assign(index, componentInfo);
			CW_ENGINE_ASSERT(res);
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

		void Render();

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