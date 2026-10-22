#include "EnginePCH.h"
#include "LightComponentBase.h"
#include "Engine/LightSceneProxy.h"
#include "Engine/Scene.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"

void ULightComponentBase::MarkSceneProxyDirty()
{
	if (SceneProxy)
		SceneProxy->GetScene()->MarkLightDirty(SceneProxy);
}

void ULightComponentBase::PostEditChangeProperty(const FProperty& Property)
{
	Super::PostEditChangeProperty(Property);
	MarkSceneProxyDirty();
}

void ULightComponentBase::OnTransformDirty()
{
	Super::OnTransformDirty();
	MarkSceneProxyDirty();
}

void ULightComponentBase::CreateRenderState()
{
	Super::CreateRenderState();

	GetOwner()->GetWorld()->GetScene().AddLight(this);
}

void ULightComponentBase::DestroyRenderState()
{
	GetOwner()->GetWorld()->GetScene().RemoveLight(this);

	Super::DestroyRenderState();
}
