#pragma once

#include "GameFramework/Actor.h"

class UBoxComponent;
class UStaticMeshComponent;

class AWall : public AActor
{
	DECLARE_CLASS(AWall, AActor)

public:
	AWall();
	UBoxComponent* GetBoxComponent() const;
	UStaticMeshComponent* GetStaticMeshComponent() const;
};
