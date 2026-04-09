#include "cwpch.h"

#include "Crowny/Scripting/Bindings/ScriptBindings.h"

#include "Crowny/Scripting/Bindings/Assets/ScriptAsset.h"
#include "Crowny/Scripting/Bindings/Assets/ScriptAudioClip.h"
#include "Crowny/Scripting/Bindings/Assets/ScriptFont.h"
#include "Crowny/Scripting/Bindings/Assets/ScriptMaterial.h"
#include "Crowny/Scripting/Bindings/Assets/ScriptMesh.h"
#include "Crowny/Scripting/Bindings/Assets/ScriptTexture.h"
#include "Crowny/Scripting/Bindings/Logging/ScriptDebug.h"
#include "Crowny/Scripting/Bindings/Math/ScriptMath.h"
#include "Crowny/Scripting/Bindings/Math/ScriptNoise.h"
#include "Crowny/Scripting/Bindings/Scene/ScriptAudioListener.h"
#include "Crowny/Scripting/Bindings/Scene/ScriptAudioSource.h"
#include "Crowny/Scripting/Bindings/Scene/ScriptCamera.h"
#include "Crowny/Scripting/Bindings/Scene/ScriptCollider2D.h"
#include "Crowny/Scripting/Bindings/Scene/ScriptEntity.h"
#include "Crowny/Scripting/Bindings/Scene/ScriptEntityBehaviour.h"
#include "Crowny/Scripting/Bindings/Scene/ScriptMeshComponent.h"
#include "Crowny/Scripting/Bindings/Scene/ScriptPhysics2D.h"
#include "Crowny/Scripting/Bindings/Scene/ScriptRigidbody.h"
#include "Crowny/Scripting/Bindings/Scene/ScriptText.h"
#include "Crowny/Scripting/Bindings/Scene/ScriptTime.h"
#include "Crowny/Scripting/Bindings/Scene/ScriptTransform.h"
#include "Crowny/Scripting/Bindings/ScriptInput.h"
#include "Crowny/Scripting/Bindings/ScriptRandom.h"
#include "Crowny/Scripting/Bindings/Utils/ScriptCompression.h"
#include "Crowny/Scripting/Bindings/Utils/ScriptFileDialog.h"
#include "Crowny/Scripting/Bindings/Utils/ScriptJSON.h"
#include "Crowny/Scripting/Bindings/Utils/ScriptLayerMask.h"

namespace Crowny
{
    void ScriptBindings::Register()
    {
        ScriptAsset::InitMetaData();
        ScriptAudioClip::InitMetaData();
        ScriptFont::InitMetaData();
        ScriptMaterial::InitMetaData();
        ScriptMesh::InitMetaData();
        ScriptTexture::InitMetaData();
        ScriptDebug::InitMetaData();
        ScriptMath::InitMetaData();
        ScriptNoise::InitMetaData();
        ScriptAudioListener::InitMetaData();
        ScriptAudioSource::InitMetaData();
        ScriptCamera::InitMetaData();
        ScriptCollider2D::InitMetaData();
        ScriptBoxCollider2D::InitMetaData();
        ScriptCircleCollider2D::InitMetaData();
        ScriptEntity::InitMetaData();
        ScriptEntityBehaviour::InitMetaData();
        ScriptMeshComponent::InitMetaData();
        ScriptPhysics2D::InitMetaData();
        ScriptRigidbody2D::InitMetaData();
        ScriptText::InitMetaData();
        ScriptTime::InitMetaData();
        ScriptTransform::InitMetaData();
        ScriptInput::InitMetaData();
        ScriptRandom::InitMetaData();
        ScriptCompression::InitMetaData();
        ScriptFileDialog::InitMetaData();
        ScriptJSON::InitMetaData();
        ScriptLayerMask::InitMetaData();
    }
} // namespace Crowny
