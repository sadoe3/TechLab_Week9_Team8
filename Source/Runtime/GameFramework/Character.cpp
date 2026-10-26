#include "EnginePCH.h"
#include "Character.h"
#include "Component/CapsuleComponent.h"
#include "Component/StaticMeshComponent.h"
#include "Component/MovementComponent.h"

ACharacter::ACharacter()
{
	USceneComponent* DefaultRoot = GetRootComponent();
	UStaticMeshComponent* Mesh = CreateDefaultSubobject<UStaticMeshComponent>("StaticMeshComponent");
	SetRootComponent(Mesh);
	// 생성 중인 미등록 기본 루트는 런타임 지연 삭제 경로를 사용하지 않는다.
	delete DefaultRoot;
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	UCapsuleComponent* Capsule = CreateDefaultSubobject<UCapsuleComponent>("CapsuleComponent");
	Capsule->SetupAttachment(Mesh);
	Capsule->SetCollisionObjectType(ECollisionChannel::Pawn);
	Capsule->SetCollisionResponseToAllChannels(ECollisionResponse::Block);
	UMovementComponent* Movement = CreateDefaultSubobject<UMovementComponent>("MovementComponent");
	Movement->SetUpdatedComponent(Mesh);
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
		Movement->SetUpdatedComponent(GetStaticMeshComponent());
		// 이동은 Character의 DuringPhysics Tick에서만 실행한다.
		Movement->PrimaryComponentTick.UnRegisterTickFunction();
		Movement->PrimaryComponentTick.bCanEverTick = false;
	}
}

void ACharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (IsPendingDestroy() || DeltaTime < 0.0f) return;
	UStaticMeshComponent* Mesh = GetStaticMeshComponent();
	UCapsuleComponent* Capsule = GetCapsuleComponent();
	UMovementComponent* Movement = GetMovementComponent();
	if (!Mesh || !Capsule || !Movement || Mesh->IsPendingDestroy() || Capsule->IsPendingDestroy() || Movement->IsPendingDestroy()) return;

	Movement->SetUpdatedComponent(Mesh);
	const FVector Delta = Movement->Velocity * DeltaTime;
	FHitResult Hit;
	const bool bMoved = Movement->SafeMoveUpdatedComponent(Delta, Mesh->GetWorldRotation(), true, Hit);
	if (IsPendingDestroy() || Mesh->IsPendingDestroy() || Capsule->IsPendingDestroy() || Movement->IsPendingDestroy()) return;
	if (bMoved && Hit.bBlockingHit)
	{
		const float VelocityIntoSurface = FVector::Dot(Movement->Velocity, Hit.Normal);
		if (VelocityIntoSurface < 0.0f) Movement->Velocity -= Hit.Normal * VelocityIntoSurface;
	}
}

UCapsuleComponent* ACharacter::GetCapsuleComponent() const
{
	return Cast<UCapsuleComponent>(FindComponentByName("CapsuleComponent"));
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
	}
}
