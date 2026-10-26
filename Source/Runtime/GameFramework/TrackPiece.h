#pragma once

#include "GameFramework/Actor.h"

class UStaticMeshComponent;

class ATrackPiece : public AActor
{
	DECLARE_CLASS(ATrackPiece, AActor)

public:
	ATrackPiece();
	UStaticMeshComponent* GetStaticMeshComponent() const;
};
