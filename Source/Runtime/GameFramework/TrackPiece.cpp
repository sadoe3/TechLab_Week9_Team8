#include "EnginePCH.h"
#include "TrackPiece.h"
#include "Component/StaticMeshComponent.h"

ATrackPiece::ATrackPiece()
{
	UStaticMeshComponent* Mesh = CreateDefaultSubobject<UStaticMeshComponent>("StaticMeshComponent");
	Mesh->SetupAttachment(GetRootComponent());
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetGenerateOverlapEvents(false);
}

UStaticMeshComponent* ATrackPiece::GetStaticMeshComponent() const
{
	return Cast<UStaticMeshComponent>(FindComponentByName("StaticMeshComponent"));
}
