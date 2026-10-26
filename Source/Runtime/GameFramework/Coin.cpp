#include "EnginePCH.h"
#include "Coin.h"
#include "Component/SphereComponent.h"
#include "Component/StaticMeshComponent.h"

ACoin::ACoin()
{
	USceneComponent* DefaultRoot = GetRootComponent();
	USphereComponent* Sphere = CreateDefaultSubobject<USphereComponent>("SphereComponent");
	SetRootComponent(Sphere);
	delete DefaultRoot;
	Sphere->SetCollisionObjectType(ECollisionChannel::WorldStatic);
	Sphere->SetCollisionResponseToAllChannels(ECollisionResponse::Overlap);
	Sphere->SetCollisionResponseToChannel(ECollisionChannel::Pawn, ECollisionResponse::Overlap);

	UStaticMeshComponent* Mesh = CreateDefaultSubobject<UStaticMeshComponent>("StaticMeshComponent");
	Mesh->SetupAttachment(Sphere);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

USphereComponent* ACoin::GetSphereComponent() const
{
	return Cast<USphereComponent>(GetRootComponent());
}

UStaticMeshComponent* ACoin::GetStaticMeshComponent() const
{
	return Cast<UStaticMeshComponent>(FindComponentByName("StaticMeshComponent"));
}
