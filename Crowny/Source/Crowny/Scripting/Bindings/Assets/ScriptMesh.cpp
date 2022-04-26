#include "cwpch.h"

#include "Crowny/Scripting/Bindings/Assets/ScriptMesh.h"

namespace Crowny
{
	ScriptMesh::ScriptMesh(MonoObject* instance, const AssetHandle<Mesh>& mesh) : TScriptAsset(instance, mesh) { }
	
	void ScriptMesh::InitRuntimeData()
	{
		
	}
}