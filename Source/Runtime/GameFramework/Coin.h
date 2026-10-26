#pragma once

#include "GameFramework/Actor.h"

class USphereComponent;
class UStaticMeshComponent;

class ACoin : public AActor
{
	DECLARE_CLASS(ACoin, AActor)

public:
	ACoin();
	USphereComponent* GetSphereComponent() const;
	UStaticMeshComponent* GetStaticMeshComponent() const;
};
