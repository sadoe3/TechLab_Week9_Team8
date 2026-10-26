#pragma once

#include "GameFramework/Actor.h"

class UBoxComponent;
class UStaticMeshComponent;
class UExponentialHeightFogComponent;

class ASwamp : public AActor
{
	DECLARE_CLASS(ASwamp, AActor)

public:
	ASwamp();
	UBoxComponent* GetBoxComponent() const;
	UStaticMeshComponent* GetStaticMeshComponent() const;
	UExponentialHeightFogComponent* GetFogComponent() const;
};
