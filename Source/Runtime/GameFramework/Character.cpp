#include "EnginePCH.h"
#include "Character.h"
#include "Component/CapsuleComponent.h"
#include "Component/StaticMeshComponent.h"
#include "Component/MovementComponent.h"

ACharacter::ACharacter()
{
	USceneComponent* DefaultRoot = GetRootComponent();
	UCapsuleComponent* Capsule = CreateDefaultSubobject<UCapsuleComponent>("CapsuleComponent");
	SetRootComponent(Capsule);
	DestroyComponent(DefaultRoot);
	Capsule->SetCollisionObjectType(ECollisionChannel::Pawn);
	Capsule->SetCollisionResponseToAllChannels(ECollisionResponse::Block);

	UStaticMeshComponent* Mesh = CreateDefaultSubobject<UStaticMeshComponent>("StaticMeshComponent");
	Mesh->SetupAttachment(Capsule);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	UMovementComponent* Movement = CreateDefaultSubobject<UMovementComponent>("MovementComponent");
	Movement->SetUpdatedComponent(Capsule);
	Movement->Velocity = FVector::ZeroVector;
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = ETickingGroup::DuringPhysics;
}

void ACharacter::BeginPlay()
{
	Super::BeginPlay();
	if (IsPendingDestroy()) return;
	if (UMovementComponent* Movement = GetMovementComponent())
	{
		Movement->SetUpdatedComponent(GetCapsuleComponent());
		// 이동은 Character의 DuringPhysics Tick에서만 실행한다.
		Movement->PrimaryComponentTick.UnRegisterTickFunction();
		Movement->PrimaryComponentTick.bCanEverTick = false;
		Movement->UpdateComponentVelocity();
	}
}

void ACharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (IsPendingDestroy() || DeltaTime < 0.0f) return;
	UCapsuleComponent* Capsule = GetCapsuleComponent();
	UMovementComponent* Movement = GetMovementComponent();
	if (!Capsule || !Movement || Capsule->IsPendingDestroy() || Movement->IsPendingDestroy()) return;

	Movement->SetUpdatedComponent(Capsule);
	const FVector Delta = Movement->Velocity * DeltaTime;
	FHitResult Hit;
	const bool bMoved = Movement->SafeMoveUpdatedComponent(Delta, Capsule->GetWorldRotation(), true, Hit);
	if (IsPendingDestroy() || Capsule->IsPendingDestroy() || Movement->IsPendingDestroy()) return;
	if (bMoved && Hit.bBlockingHit)
	{
		const float VelocityIntoSurface = FVector::Dot(Movement->Velocity, Hit.Normal);
		if (VelocityIntoSurface < 0.0f) Movement->Velocity -= Hit.Normal * VelocityIntoSurface;
	}
	Movement->UpdateComponentVelocity();
}

UCapsuleComponent* ACharacter::GetCapsuleComponent() const
{
	return Cast<UCapsuleComponent>(GetRootComponent());
}

UStaticMeshComponent* ACharacter::GetStaticMeshComponent() const
{
	return Cast<UStaticMeshComponent>(FindComponentByName("StaticMeshComponent"));
}

UMovementComponent* ACharacter::GetMovementComponent() const
{
	return Cast<UMovementComponent>(FindComponentByName("MovementComponent"));
}

FVector ACharacter::GetVelocity() const
{
	const UMovementComponent* Movement = GetMovementComponent();
	return Movement ? Movement->Velocity : FVector::ZeroVector;
}

void ACharacter::SetVelocity(const FVector& NewVelocity)
{
	if (IsPendingDestroy()) return;
	if (UMovementComponent* Movement = GetMovementComponent())
	{
		Movement->Velocity = NewVelocity;
		Movement->UpdateComponentVelocity();
	}
}
