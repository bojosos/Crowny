#include "cwpch.h"

#include "Crowny/Scripting/Bindings/Assets/ScriptAnimationClip.h"

namespace Crowny
{
    ScriptAnimationClip::ScriptAnimationClip(MonoObject* instance, const AssetHandle<AnimationClip>& clip) : TScriptAsset(instance, clip) {}

    void ScriptAnimationClip::InitRuntimeData() {}

} // namespace Crowny
