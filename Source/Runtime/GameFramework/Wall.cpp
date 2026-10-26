#include "EnginePCH.h"
#include "Wall.h"
#include "Component/BoxComponent.h"
#include "Component/StaticMeshComponent.h"

AWall::AWall()
{
	USceneComponent* DefaultRoot = GetRootComponent();
	UBoxComponent* Box = CreateDefaultSubobject<UBoxComponent>("BoxComponent");
	SetRootComponent(Box);
	delete DefaultRoot;
	Box->SetCollisionObjectType(ECollisionChannel::WorldStatic);
	Box->SetCollisionResponseToAllChannels(ECollisionResponse::Block);
	Box->SetCollisionResponseToChannel(ECollisionChannel::Pawn, ECollisionResponse::Block);

	UStaticMeshComponent* Mesh = CreateDefaultSubobject<UStaticMeshComponent>("StaticMeshComponent");
	Mesh->SetupAttachment(Box);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

UBoxComponent* AWall::GetBoxComponent() const
{
	return Cast<UBoxComponent>(GetRootComponent());
}

UStaticMeshComponent* AWall::GetStaticMeshComponent() const
{
	return Cast<UStaticMeshComponent>(FindComponentByName("StaticMeshComponent"));
}
