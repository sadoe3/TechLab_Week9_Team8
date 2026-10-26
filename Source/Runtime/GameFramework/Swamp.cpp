#include "EnginePCH.h"
#include "Swamp.h"
#include "Component/BoxComponent.h"
#include "Component/StaticMeshComponent.h"
#include "Component/ScriptComponent.h"

ASwamp::ASwamp()
{
	USceneComponent* DefaultRoot = GetRootComponent();
	UStaticMeshComponent* Mesh = CreateDefaultSubobject<UStaticMeshComponent>("StaticMeshComponent");
	SetRootComponent(Mesh);
	delete DefaultRoot;
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	UBoxComponent* Box = CreateDefaultSubobject<UBoxComponent>("BoxComponent");
	Box->SetupAttachment(Mesh);
	Box->SetCollisionObjectType(ECollisionChannel::WorldStatic);
	Box->SetCollisionResponseToAllChannels(ECollisionResponse::Ignore);
	Box->SetCollisionResponseToChannel(ECollisionChannel::Pawn, ECollisionResponse::Overlap);

	// BeginPlay에서 ScriptComponent가 Box의 Overlap 이벤트를 Lua 콜백에 연결한다.
	UScriptComponent* Script = CreateDefaultSubobject<UScriptComponent>("SwampScript");
	Script->SetScriptPath("Assets/Scripts/Swamp.lua");
}

UBoxComponent* ASwamp::GetBoxComponent() const
{
	return Cast<UBoxComponent>(FindComponentByName("BoxComponent"));
}

UStaticMeshComponent* ASwamp::GetStaticMeshComponent() const
{
	return Cast<UStaticMeshComponent>(GetRootComponent());
}
