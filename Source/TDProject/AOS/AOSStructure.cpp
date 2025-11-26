#include "AOSStructure.h"
#include "AOSCharacter.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SphereComponent.h"
#include "EngineUtils.h"
#include "Materials/Material.h"

AAOSStructure::AAOSStructure()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;

	// 루트 컴포넌트 설정
	CollisionComponent = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionComponent"));
	RootComponent = CollisionComponent;
	CollisionComponent->SetSphereRadius(100.0f);

	// 메시 컴포넌트 설정
	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
	MeshComponent->SetupAttachment(RootComponent);

	// 감지 범위 설정
	DetectionRange = CreateDefaultSubobject<USphereComponent>(TEXT("DetectionRange"));
	DetectionRange->SetupAttachment(RootComponent);
	DetectionRange->SetSphereRadius(1500.0f);

	CurrentHealth = MaxHealth;
}

void AAOSStructure::BeginPlay()
{
	Super::BeginPlay();

	CurrentHealth = MaxHealth;
}

void AAOSStructure::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!IsDestroyed())
	{
		UpdateAttackTarget();

		// 공격 쿨타임 업데이트 및 공격 실행
		if (CurrentAttackCooldown > 0.0f)
		{
			CurrentAttackCooldown -= DeltaTime;
		}
		else if (CurrentTarget && CurrentTarget->IsAlive())
		{
			FireAtTarget(CurrentTarget);
			CurrentAttackCooldown = AttackCooldown;
		}
	}
}

void AAOSStructure::Initialize(EStructureType Type, EAOSTeam InOwnerTeam, EAOSLane InLane)
{
	StructureType = Type;
	OwnerTeam = InOwnerTeam;
	Lane = InLane;

	// 구조물 종류에 따라 최대 체력 및 메시 설정
	if (Type == EStructureType::CommandCenter)
	{
		MaxHealth = 5000.0f;
		SetupCommandCenterMesh();
	}
	else if (Type == EStructureType::Tower)
	{
		MaxHealth = 1000.0f;
		SetupTowerMesh();
	}

	CurrentHealth = MaxHealth;
}

void AAOSStructure::SetupTowerMesh()
{
	if (!MeshComponent)
	{
		return;
	}

	// 🟡 MODIFIED - ConstructorHelpers 대신 LoadObject 사용 (런타임 안전)
	UStaticMesh* CubeMesh = LoadObject<UStaticMesh>(nullptr, TEXT("StaticMesh'/Engine/BasicShapes/Cube.Cube'"));

	if (CubeMesh)
	{
		MeshComponent->SetStaticMesh(CubeMesh);

		// 타워 스케일 설정
		MeshComponent->SetRelativeScale3D(FVector(1.0f, 1.0f, 2.0f));

		// 동적 머티리얼 생성
		UMaterial* BaseMaterial = LoadObject<UMaterial>(nullptr, TEXT("Material'/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial'"));

		FLinearColor TowerColor = (OwnerTeam == EAOSTeam::Team1) ? FLinearColor::Red : FLinearColor::Blue;

		if (BaseMaterial)
		{
			UMaterialInstanceDynamic* DynamicMaterial = UMaterialInstanceDynamic::Create(BaseMaterial, MeshComponent);

			// BaseColor 파라미터 시도
			DynamicMaterial->SetVectorParameterValue(FName("BaseColor"), TowerColor);

			// 더불어 Emissive도 설정 (밝게 표시)
			DynamicMaterial->SetVectorParameterValue(FName("Emissive"), TowerColor * 0.5f);

			MeshComponent->SetMaterial(0, DynamicMaterial);
			UE_LOG(LogTemp, Warning, TEXT("Tower mesh setup with dynamic material: Team=%d, Color=(%.1f,%.1f,%.1f)"),
				static_cast<int32>(OwnerTeam), TowerColor.R, TowerColor.G, TowerColor.B);
		}
		else
		{
			// 머티리얼 로드 실패 - 기본 엔진 머티리얼로 폴백
			UE_LOG(LogTemp, Warning, TEXT("BasicShapeMaterial not found, using default material for tower"));
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to load cube mesh for tower"));
	}
}

void AAOSStructure::SetupCommandCenterMesh()
{
	if (!MeshComponent)
	{
		return;
	}

	// 🟡 MODIFIED - ConstructorHelpers 대신 LoadObject 사용 (런타임 안전)
	UStaticMesh* CubeMesh = LoadObject<UStaticMesh>(nullptr, TEXT("StaticMesh'/Engine/BasicShapes/Cube.Cube'"));

	if (CubeMesh)
	{
		MeshComponent->SetStaticMesh(CubeMesh);

		// 커맨드 센터 스케일 설정 (더 크게)
		MeshComponent->SetRelativeScale3D(FVector(2.0f, 2.0f, 3.0f));

		// 동적 머티리얼 생성
		UMaterial* BaseMaterial = LoadObject<UMaterial>(nullptr, TEXT("Material'/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial'"));

		FLinearColor CenterColor = (OwnerTeam == EAOSTeam::Team1) ? FLinearColor(1.0f, 0.5f, 0.5f, 1.0f) : FLinearColor(0.5f, 0.5f, 1.0f, 1.0f);

		if (BaseMaterial)
		{
			UMaterialInstanceDynamic* DynamicMaterial = UMaterialInstanceDynamic::Create(BaseMaterial, MeshComponent);

			// BaseColor 파라미터 시도
			DynamicMaterial->SetVectorParameterValue(FName("BaseColor"), CenterColor);

			// 더불어 Emissive도 설정 (밝게 표시)
			DynamicMaterial->SetVectorParameterValue(FName("Emissive"), CenterColor * 0.3f);

			MeshComponent->SetMaterial(0, DynamicMaterial);
			UE_LOG(LogTemp, Warning, TEXT("Command Center mesh setup with dynamic material: Team=%d, Color=(%.1f,%.1f,%.1f)"),
				static_cast<int32>(OwnerTeam), CenterColor.R, CenterColor.G, CenterColor.B);
		}
		else
		{
			// 머티리얼 로드 실패 - 기본 엔진 머티리얼로 폴백
			UE_LOG(LogTemp, Warning, TEXT("BasicShapeMaterial not found, using default material for command center"));
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to load cube mesh for command center"));
	}
}

void AAOSStructure::ReceiveDamage(float DamageAmount)
{
	CurrentHealth = FMath::Max(0.0f, CurrentHealth - DamageAmount);

	if (IsDestroyed())
	{
		// 구조물 파괴 처리
		// TODO: 파괴 애니메이션, 이펙트, 게임 규칙 처리 등
	}
}

void AAOSStructure::FireAtTarget(AAOSCharacter* Target)
{
	if (!Target || !Target->IsAlive())
	{
		CurrentTarget = nullptr;
		return;
	}

	float Distance = FVector::Dist(GetActorLocation(), Target->GetActorLocation());
	if (Distance <= AttackRange)
	{
		Target->ReceiveDamage(AttackDamage);
	}
}

AAOSCharacter* AAOSStructure::FindNearestEnemy()
{
	float NearestDistance = FLT_MAX;
	AAOSCharacter* NearestEnemy = nullptr;
	float DetectionRadius = 1500.0f;

	// 월드의 모든 AAOSCharacter를 순회
	for (TActorIterator<AAOSCharacter> ActorItr(GetWorld()); ActorItr; ++ActorItr)
	{
		AAOSCharacter* Character = *ActorItr;
		if (Character && Character->GetTeam() != OwnerTeam && Character->IsAlive())
		{
			float Distance = FVector::Dist(GetActorLocation(), Character->GetActorLocation());
			if (Distance < NearestDistance && Distance <= DetectionRadius)
			{
				NearestDistance = Distance;
				NearestEnemy = Character;
			}
		}
	}

	return NearestEnemy;
}

void AAOSStructure::UpdateAttackTarget()
{
	// 현재 목표가 없거나 죽었다면 새로운 목표 찾기
	if (!CurrentTarget || !CurrentTarget->IsAlive())
	{
		CurrentTarget = FindNearestEnemy();
	}
}
