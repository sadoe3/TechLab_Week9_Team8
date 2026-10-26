#include "EnginePCH.h"
#include "Component/ParticleSubUVComponent.h"
#include "Asset/AssetManager.h"
#include "Render/RenderCommand.h"

#include "Camera/CameraComponent.h"

#include "BillboardComponent.h"
#include "UObject/Property.h"
#include <algorithm>
#include "Engine/World.h"
#include <cmath>

const float UParticleSubUVComponent::MAX_NORMALIZED_VALUE = 1.0f;

UParticleSubUVComponent::UParticleSubUVComponent()
{
	// 파티클 시뮬레이션을 매 프레임 진행한다.
	PrimaryComponentTick.bCanEverTick = true;

	ColSize = 8;
	RowSize = 8;
	FrameRate = 12.0f;

	SetHiddenInGame(false);

	// Todo: Default setting, set value from arguments
	//AtlasTexturePath = "ParticleAtlas.png";
	
	// AtlasTexture = nullptr;

	Material = UAssetManager::GetAssetByPath<UMaterial>("SubUVMaterial");
}


void UParticleSubUVComponent::BeginPlay()
{
	Super::BeginPlay();
	InitializeParticles();
}

// 열·행 개수가 양수인지 검사해 SubUV 분할 수를 설정한다.
void UParticleSubUVComponent::SetSubUVSize(uint32 NewColSize, uint32 NewRowSize)
{
	assert(NewColSize > 0 && NewRowSize > 0);

	ColSize = NewColSize;
	RowSize = NewRowSize;
}

// 유효한 프레임 속도를 저장하고 음수·0이면 기본값을 쓴다.
void UParticleSubUVComponent::SetFrameRate(float InFrameRate)
{
	FrameRate = (InFrameRate > 0.0f) ? InFrameRate : 1.0f;
}

// DeltaTime으로 이동·수명·Atlas 프레임을 갱신한다.
void UParticleSubUVComponent::TickComponent(float DeltaTime)
{
	Super::TickComponent(DeltaTime);
	const uint32 TotalFrames = ColSize * RowSize;
	// 입자 경계 변경 여부
	bool bBoundsChanged = false;

	for (FParticle& Particle : Particles)
	{
		// 비활성 슬롯 제외
		if (!Particle.bAlive)
		{
			continue;
		}
		bBoundsChanged = true;
		Particle.Location += Particle.Velocity * DeltaTime;
		Particle.Age += DeltaTime;

		if (Particle.Age >= Particle.LifeTime)
		{
			if (bLooping)
			{
				RespawnParticle(Particle);
			}
			else
			{
				Particle.bAlive = false;
			}
			continue;
		}

		const float LifeRatio = Particle.Age / Particle.LifeTime;
		
		Particle.Scale = Lerp(StartScale, EndScale, LifeRatio);

		if (LifeRatio > FadeStart)
		{
			const float FadeRatio = GedSmoothStepedRatio(FadeStart, MAX_NORMALIZED_VALUE, LifeRatio);
			Particle.Alpha = MAX_NORMALIZED_VALUE - FadeRatio;
		}
		else
		{
			Particle.Alpha = MAX_NORMALIZED_VALUE;
		}

		Particle.FrameTimer += DeltaTime;
		const float FrameDuration = MAX_NORMALIZED_VALUE / FrameRate;

		if (Particle.FrameTimer >= FrameDuration)
		{
			Particle.FrameTimer = 0.0f;

			++Particle.SubUVFrame;
			Particle.SubUVFrame %= TotalFrames;
		}
	}
	if (bBoundsChanged)
	{
		MarkTransformDirty();
	}
}

// 파티클 에셋과 배열을 준비하고 초기 상태를 채운다.
void UParticleSubUVComponent::OnRegister()
{
	if (!Material)
	{
		Material = UAssetManager::GetAssetByPath<UMaterial>("SubUVMaterial");
	}
	Super::OnRegister();
	InitializeParticles();
}

// 재생 설정에 따른 파티클 슬롯 초기화
void UParticleSubUVComponent::InitializeParticles()
{
	// 배열·Atlas 크기와 프레임 속도 보정
	ParticleCount = std::max(0, ParticleCount);
	ColSize = std::max(1u, ColSize);
	RowSize = std::max(1u, RowSize);
	SetFrameRate(FrameRate);
	// 공개 튜닝 범위 보정
	MinLifeTime = std::max(0.001f, MinLifeTime);
	MaxLifeTime = std::max(MinLifeTime, MaxLifeTime);
	StartScale = std::max(0.0f, StartScale);
	EndScale = std::max(0.0f, EndScale);
	FadeStart = std::clamp(FadeStart, 0.0f, 1.0f);
	MinBurstSpeed = std::max(0.0f, MinBurstSpeed);
	MaxBurstSpeed = std::max(MinBurstSpeed, MaxBurstSpeed);
	BurstSpreadAngle = std::clamp(BurstSpreadAngle, 0.0f, 90.0f);
	Particles.Reset();
	Particles.Reserve(ParticleCount);

	const uint32 TotalFrames = ColSize * RowSize;
	for (int32 Index = 0; Index < ParticleCount; ++Index)
	{
		// 비활성 슬롯의 전체 필드 초기화
		FParticle Particle{};

		if (bLooping)
		{
			// 기존 Loop의 분산된 초기 상태 유지
			RespawnParticle(Particle);
			Particle.Age = GetRandomNumberBetween(0.0f, Particle.LifeTime);
			Particle.Location.Z += Particle.Velocity.Z * Particle.Age;

			const float LifeRatio = Particle.Age / Particle.LifeTime;
			Particle.Scale = Lerp(StartScale, EndScale, LifeRatio);

			const float ElapsedFrames = Particle.Age * FrameRate;
			Particle.SubUVFrame = static_cast<uint32>(ElapsedFrames) % TotalFrames;
		}

		Particles.Add(Particle);
	}
	MarkTransformDirty();
}

// 기본 카메라 기준으로 파티클 상수와 렌더 패킷을 구성한다.
void UParticleSubUVComponent::SubmitToRenderQueue(FRenderQueue& RenderQueue)
{
	assert(QuadMesh != nullptr);
	assert(Material != nullptr);
	
	Constants.Reset();
	Constants.Reserve(Particles.Num());

	const FVector CameraPos = GetOwner()->GetWorld()->GetMainCamera()->GetWorldLocation();
	for (FParticle& Particle : Particles)
	{
		if (Particle.bAlive == false)
		{
			continue;
		}


		FMatrix WorldMatrix = FMatrix::Identity; 
		Super::GetWorldTransformedMatrix(&WorldMatrix);

		// Scale, Move
		WorldMatrix.M[0][0] *= Particle.Scale;
		WorldMatrix.M[0][1] *= Particle.Scale;
		WorldMatrix.M[0][2] *= Particle.Scale;
		WorldMatrix.M[0][3] *= Particle.Scale;

		WorldMatrix.M[1][0] *= Particle.Scale;
		WorldMatrix.M[1][1] *= Particle.Scale;
		WorldMatrix.M[1][2] *= Particle.Scale;
		WorldMatrix.M[1][3] *= Particle.Scale;

		WorldMatrix.M[2][0] *= Particle.Scale;
		WorldMatrix.M[2][1] *= Particle.Scale;
		WorldMatrix.M[2][2] *= Particle.Scale;
		WorldMatrix.M[2][3] *= Particle.Scale;

		WorldMatrix.M[3][0] = Particle.Location.X;
		WorldMatrix.M[3][1] = Particle.Location.Y;
		WorldMatrix.M[3][2] = Particle.Location.Z;
		WorldMatrix.M[3][3] = 1.0f;

		const FVector ParticlePos = Particle.Location;
		const FVector CameraToParticleVec = ParticlePos - CameraPos;

		const float CameraToParticleDistance = 
			CameraToParticleVec.X *
			CameraToParticleVec.X +

			CameraToParticleVec.Y *
			CameraToParticleVec.Y +

			CameraToParticleVec.Z *
			CameraToParticleVec.Z;

		FRenderPacket Packet;
		Packet.Model = RenderQueue.StoreWorldMatrix(WorldMatrix);
		Packet.Mesh = QuadMesh;
		Packet.Material = Material;

		FSubUVConstants C;
		C.CurrentFrame =  Particle.SubUVFrame;
		C.AtlasColSize = ColSize;
		C.AtlasRowSize = RowSize;
		C.Alpha = Particle.Alpha;

		Constants.Add(C);

		Packet.MaterialParamData = &Constants.Last();
		Packet.MaterialParamDataSize = sizeof(FSubUVConstants);

		Packet.CameraToParticleDistance = CameraToParticleDistance;

		RenderQueue.Add(Packet);
	}
}

// 파티클 위치·속도·수명 등 재생성 상태를 초기화한다.
void UParticleSubUVComponent::RespawnParticle(FParticle& Particle)
{
	Particle.Location = GetWorldLocation();

	Particle.Velocity.X = 0.0f;
	Particle.Velocity.Y = 0.0f;
	Particle.Velocity.Z = GetRandomNumberBetween(MinRiseSpeed, MaxRiseSpeed);

	Particle.LifeTime = GetRandomNumberBetween(MinLifeTime, MaxLifeTime);
	Particle.Age = 0.0f;
	Particle.SubUVFrame = static_cast<uint32>(GetRandomNumberBetween(0.0f, static_cast<float>((ColSize * RowSize) - 1)));
	Particle.Scale = StartScale;
	Particle.FrameTimer = 0.0f;
	Particle.bAlive = true;

	Particle.Alpha = MAX_NORMALIZED_VALUE;
}

// Todo: Move to util class
// 난수를 지정 실수 구간으로 변환한다.
float UParticleSubUVComponent::GetRandomNumberBetween(float start, float end) const
{
	float randomNumber = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);

	return start + randomNumber * (end - start);
}

// 두 값을 alpha 비율로 선형 보간한다.
float UParticleSubUVComponent::Lerp(float left, float right, float alpha) const
{
	//assert(alpha >= 0.0f && alpha <= 1.0f);
	if (alpha < 0.0f)
	{
		alpha = 0.0f;
	}
	
	if (alpha > 1.0f)
	{
		alpha = 1.0f;
	}

	return left + (right - left) * alpha;
}

// 정규화한 구간 비율에 삼차 smoothstep을 적용한다.
float UParticleSubUVComponent::GedSmoothStepedRatio(float start, float end, float value) const
{
	float normedValue = (value - start) / (end - start);
	//assert(normedValue >= 0.0f && normedValue <= 1.0f);

	if (normedValue < 0.0f)
	{
		normedValue = 0.0f;
	}

	if (normedValue > 1.0f)
	{
		normedValue = 1.0f;
	}

	return normedValue * normedValue * (3.0f - 2.0f * normedValue);
}

// 프레임당 한 번 전체 인덱스 순서로 상수를 준비하며 View 제출 중 재할당하지 않는다.
void UParticleSubUVComponent::BeginViewSubmission()
{
	Constants.Reset();
	Constants.Reserve(Particles.Num());
    for (const FParticle& Particle : Particles)
    {
        FSubUVConstants Value{};
        Value.CurrentFrame = static_cast<float>(Particle.SubUVFrame);
        Value.AtlasColSize = static_cast<float>(ColSize);
        Value.AtlasRowSize = static_cast<float>(RowSize);
        Value.Alpha = Particle.Alpha;
        Constants.Add(Value);
    }
}

// 파티클의 View 행렬·거리·SubUV 상수를 렌더 패킷에 담는다.
void UParticleSubUVComponent::SubmitParticleToRenderQueue(
	FRenderQueue& RenderQueue,
	const int32 ParticleIndex,
	const FMatrix& WorldMatrix,
	const float CameraDistanceSquared)
{
	if (ParticleIndex < 0 || ParticleIndex >= Particles.Num() || QuadMesh == nullptr || Material == nullptr)
		return;

	const FParticle& Particle = Particles[ParticleIndex];
	if (!Particle.bAlive)
		return;

	assert(Constants.Num() == Particles.Num());

	FRenderPacket Packet;
	Packet.Model = RenderQueue.StoreWorldMatrix(WorldMatrix);
	Packet.Mesh = QuadMesh;
	Packet.Material = Material;
	Packet.CameraToParticleDistance = CameraDistanceSquared;
	Packet.MaterialParamData = &Constants[ParticleIndex];
	Packet.MaterialParamDataSize = sizeof(FSubUVConstants);
	RenderQueue.Add(Packet);
}


// 실제 재질의 블렌드 상태로 프레임 공통 정렬과 View별 정렬을 구분한다.
bool UParticleSubUVComponent::UsesOpaqueMaterial() const
{
    return Material && Material->BlendState == EBlendState::Opaque;
}

// 저장된 재생 설정에 따른 슬롯 초기화
void UParticleSubUVComponent::Serialize(FStructuredArchive::FRecord Record)
{
	Super::Serialize(Record);

	if (Record.GetUnderlyingArchive().IsLoading())
	{
		InitializeParticles();
	}
}

// 에디터 재생 설정 변경 반영
void UParticleSubUVComponent::PostEditChangeProperty(const FProperty& Property)
{
	Super::PostEditChangeProperty(Property);

	if (Property.Name == "bLooping" || Property.Name == "ParticleCount" ||
		Property.Name == "ColSize" || Property.Name == "RowSize" ||
		Property.Name == "FrameRate" || Property.Name == "MinLifeTime" ||
		Property.Name == "MaxLifeTime" || Property.Name == "StartScale" ||
		Property.Name == "EndScale" || Property.Name == "FadeStart" ||
		Property.Name == "MinBurstSpeed" || Property.Name == "MaxBurstSpeed" ||
		Property.Name == "BurstSpreadAngle")
	{
		InitializeParticles();
	}
}

// 비활성 슬롯을 재사용한 원뿔 방향 방출
int32 UParticleSubUVComponent::EmitBurst(const FVector& Position, const FVector& Direction, int32 Count)
{
	if (bLooping || Count <= 0 || !CanRunGameplay())
	{
		return 0;
	}

	const FVector Axis = Direction.GetSafeNormal();
	if (Axis.SizeSquared() == 0.0f)
	{
		return 0;
	}

	// 방출 축의 직교 기저
	const FVector Reference = std::abs(Axis.Z) < 0.999f ? FVector(0, 0, 1) : FVector(0, 1, 0);
	const FVector Tangent = FVector::Cross(Reference, Axis).GetSafeNormal();
	const FVector Bitangent = FVector::Cross(Axis, Tangent);
	const float MinimumCosine = std::cos(FMath::DegreesToRadians(BurstSpreadAngle));

	int32 EmittedCount = 0;
	for (FParticle& Particle : Particles)
	{
		if (Particle.bAlive)
		{
			continue;
		}

		// 기존 입자 초기화 재사용
		RespawnParticle(Particle);

		const float Cosine = GetRandomNumberBetween(MinimumCosine, 1.0f);
		const float Sine = std::sqrt(std::max(0.0f, 1.0f - Cosine * Cosine));
		const float Angle = GetRandomNumberBetween(0.0f, 2.0f * PI);
		const FVector VelocityDirection = Axis * Cosine + Tangent * (Sine * std::cos(Angle)) +
			Bitangent * (Sine * std::sin(Angle));

		Particle.Location = Position;
		Particle.Velocity = VelocityDirection *	GetRandomNumberBetween(MinBurstSpeed, MaxBurstSpeed);
		Particle.SubUVFrame = 0;

		++EmittedCount;
		if (EmittedCount >= Count)
		{
			break;
		}
	}

	if (EmittedCount > 0)
	{
		MarkTransformDirty();
	}

	return EmittedCount;
}

// 슬롯 용량을 유지한 활성 입자 정리
void UParticleSubUVComponent::ClearParticles()
{
	for (FParticle& Particle : Particles)
	{
		Particle.bAlive = false;
		Particle.Alpha = 0.0f;
	}

	MarkTransformDirty();
}

// 활성 입자와 Billboard 크기를 포함하는 월드 경계
FBox UParticleSubUVComponent::CalcBounds() const
{
	float QuadRadius = 1.0f;
	if (const FStaticMeshData* MeshData = GetMeshData())
	{
		const FBox& LocalBounds = MeshData->AABB;
		const float HalfWidth =	std::max(std::abs(LocalBounds.Min.Y), std::abs(LocalBounds.Max.Y));
		const float HalfHeight = std::max(std::abs(LocalBounds.Min.Z), std::abs(LocalBounds.Max.Z));
		QuadRadius = std::sqrt(HalfWidth * HalfWidth + HalfHeight * HalfHeight);
	}

	FBox Bounds{};
	bool bHasAliveParticle = false;

	for (const FParticle& Particle : Particles)
	{
		if (!Particle.bAlive)
		{
			continue;
		}

		const FVector Extent(QuadRadius * std::abs(Particle.Scale));
		const FBox ParticleBounds{Particle.Location - Extent, Particle.Location + Extent};

		if (!bHasAliveParticle)
		{
			Bounds = ParticleBounds;
			bHasAliveParticle = true;
		}
		else
		{
			Bounds.Expand(ParticleBounds);
		}
	}

	return bHasAliveParticle ? Bounds : FBox{ GetWorldLocation(), GetWorldLocation() };
}