#include  "EnginePCH.h"
#include "ExponentialHeightFogComponent.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"

FFogInfo UExponentialHeightFogComponent::GetFogInfo() const
{
	FFogInfo FogInfo;
	FogInfo.FogDensity = FogDensity;
	FogInfo.FogHeightFalloff = FogHeightFalloff;
	FogInfo.StartDistance = StartDistance;
	FogInfo.FogCutoffDistance = FogCutoffDistance;
	FogInfo.FogMaxOpacity = FogMaxOpacity;
	FogInfo.FogColor = FVector4(FogColor.R, FogColor.G, FogColor.B, FogColor.A);
	FogInfo.fogHeight = GetOwner()->GetActorLocation().Z;

	return FogInfo;
}

void UExponentialHeightFogComponent::SetFogEnabled(bool bEnabled)
{
	bFogEnabled = bEnabled;
	if (!IsRegistered()) return;
	FScene& Scene = GetOwner()->GetWorld()->GetScene();
	Scene.RemoveFogInfo(GetUUID());
	if (bFogEnabled) Scene.AddFogInfo(GetUUID(), GetFogInfo());
}

void UExponentialHeightFogComponent::PostEditChangeProperty(const FProperty& Property)
{
	Super::PostEditChangeProperty(Property);
	if (Property.Name == "bFogEnabled")
	{
		SetFogEnabled(bFogEnabled);
		return;
	}
	UpdateSceneFogInfo();
}

void UExponentialHeightFogComponent::OnTransformDirty()
{
	Super::OnTransformDirty();
	UpdateSceneFogInfo();
}

void UExponentialHeightFogComponent::CreateRenderState()
{
	Super::CreateRenderState();

	if (bFogEnabled) GetOwner()->GetWorld()->GetScene().AddFogInfo(GetUUID(), GetFogInfo());
}

void UExponentialHeightFogComponent::DestroyRenderState()
{
	GetOwner()->GetWorld()->GetScene().RemoveFogInfo(GetUUID());

	Super::DestroyRenderState();
}

void UExponentialHeightFogComponent::UpdateSceneFogInfo()
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor) return;

	UWorld* World = OwnerActor->GetWorld();
	if (!World) return;

	World->GetScene().UpdateFogInfo(GetUUID(), GetFogInfo());
}
