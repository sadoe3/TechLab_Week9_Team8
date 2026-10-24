#pragma once

#include "GameFramework/Actor.h"

class UCapsuleComponent;
class UStaticMeshComponent;
class UMovementComponent;

class ACharacter : public AActor
{
	DECLARE_CLASS(ACharacter, AActor)

public:
	ACharacter();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	UCapsuleComponent* GetCapsuleComponent() const;
	UStaticMeshComponent* GetStaticMeshComponent() const;
	UMovementComponent* GetMovementComponent() const;
	FVector GetVelocity() const;
	void SetVelocity(const FVector& NewVelocity);
};
