#include "cwpch.h"

#include "Crowny/Scripting/Bindings/ScriptBindings.h"

#include "Crowny/Scripting/Bindings/Assets/ScriptAsset.h"
#include "Crowny/Scripting/Bindings/Assets/ScriptAssetDatabase.h"
#include "Crowny/Scripting/Bindings/Assets/ScriptAudioClip.h"
#include "Crowny/Scripting/Bindings/Assets/ScriptAudioMixer.h"
#include "Crowny/Scripting/Bindings/Assets/ScriptFont.h"
#include "Crowny/Scripting/Bindings/Assets/ScriptMaterial.h"
#include "Crowny/Scripting/Bindings/Assets/ScriptMesh.h"
#include "Crowny/Scripting/Bindings/Assets/ScriptPhysicsMaterial2D.h"
#include "Crowny/Scripting/Bindings/Assets/ScriptPhysicsMaterial3D.h"
#include "Crowny/Scripting/Bindings/Assets/ScriptTexture.h"
#include "Crowny/Scripting/Bindings/Logging/ScriptDebug.h"
#include "Crowny/Scripting/Bindings/Math/ScriptMath.h"
#include "Crowny/Scripting/Bindings/Math/ScriptNoise.h"
#include "Crowny/Scripting/Bindings/Scene/ScriptAudioListener.h"
#include "Crowny/Scripting/Bindings/Scene/ScriptAudioSource.h"
#include "Crowny/Scripting/Bindings/Scene/ScriptCamera.h"
#include "Crowny/Scripting/Bindings/Scene/ScriptCollider2D.h"
#include "Crowny/Scripting/Bindings/Scene/ScriptCollider3D.h"
#include "Crowny/Scripting/Bindings/Scene/ScriptEntity.h"
#include "Crowny/Scripting/Bindings/Scene/ScriptEntityBehaviour.h"
#include "Crowny/Scripting/Bindings/Scene/ScriptLight.h"
#include "Crowny/Scripting/Bindings/Scene/ScriptMeshComponent.h"
#include "Crowny/Scripting/Bindings/Scene/ScriptPhysics2D.h"
#include "Crowny/Scripting/Bindings/Scene/ScriptPhysics3D.h"
#include "Crowny/Scripting/Bindings/Scene/ScriptRigidbody.h"
#include "Crowny/Scripting/Bindings/Scene/ScriptRigidbody3D.h"
#include "Crowny/Scripting/Bindings/Scene/ScriptSceneManager.h"
#include "Crowny/Scripting/Bindings/Scene/ScriptSpriteRenderer.h"
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
        ScriptAssetDatabase::InitMetaData();
        ScriptAudioClip::InitMetaData();
        ScriptAudioMixer::InitMetaData();
        ScriptFont::InitMetaData();
        ScriptMaterial::InitMetaData();
        ScriptMesh::InitMetaData();
        ScriptPhysicsMaterial2D::InitMetaData();
        ScriptPhysicsMaterial3D::InitMetaData();
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
        ScriptCollider3D::InitMetaData();
        ScriptBoxCollider3D::InitMetaData();
        ScriptSphereCollider3D::InitMetaData();
        ScriptCapsuleCollider3D::InitMetaData();
        ScriptEntity::InitMetaData();
        ScriptEntityBehaviour::InitMetaData();
        ScriptLight::InitMetaData();
        ScriptMeshComponent::InitMetaData();
        ScriptPhysics2D::InitMetaData();
        ScriptPhysics3D::InitMetaData();
        ScriptRigidbody2D::InitMetaData();
        ScriptRigidbody3D::InitMetaData();
        ScriptSceneManager::InitMetaData();
        ScriptSpriteRenderer::InitMetaData();
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
