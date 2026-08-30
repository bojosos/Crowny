#pragma once

#include "Crowny/Animation/AnimationClip.h"
#include "Crowny/Scripting/Bindings/Assets/ScriptAsset.h"

namespace Crowny
{
    class ScriptAnimationClip : public TScriptAsset<ScriptAnimationClip, AnimationClip>
    {
    public:
        SCRIPT_WRAPPER(CROWNY_ASSEMBLY, CROWNY_NS, "AnimationClip");
        ScriptAnimationClip(MonoObject* instance, const AssetHandle<AnimationClip>& clip);

    private:
    };
} // namespace Crowny
