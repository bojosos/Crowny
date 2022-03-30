#include "cwpch.h"

#include "Crowny/Assets/AssetHandle.h"

namespace Crowny
{

	void AssetHandleBase::Destroy()
	{
		//if (m_Data*->m_Ptr != nullptr)
			//AssetManager::Get().Destroy();
	}

	void AssetHandleBase::SetHandleData(const Ref<Asset>& ptr, const UUID& uuid)
	{
		m_Data->m_Ptr = ptr;
		if (m_Data->m_Ptr != nullptr)
			m_Data->m_UUID = uuid;
	}
}