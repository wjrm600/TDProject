#include "AOSStructure.h"
#include "AOSCharacter.h"
#include "UI/AOSHealthBarWidget.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Components/WidgetComponent.h"
#include "EngineUtils.h"
#include "Materials/Material.h"
#include "GameFramework/PlayerController.h"
#include "Net/UnrealNetwork.h"
#include "DrawDebugHelpers.h"
#include "HAL/IConsoleManager.h"

AAOSStructure::AAOSStructure()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;

	// 루트 컴포넌트 설정
	CollisionComponent = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionComponent"));
	RootComponent = CollisionComponent;
	CollisionComponent->SetSphereRadius(100.0f);
	CollisionComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);  // 콜리전 비활성화

	// 메시 컴포넌트 설정
	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
	MeshComponent->SetupAttachment(RootComponent);
	MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);  // 메시 콜리전 비활성화

	// 감지 범위 설정
	DetectionRange = CreateDefaultSubobject<USphereComponent>(TEXT("DetectionRange"));
	DetectionRange->SetupAttachment(RootComponent);
	DetectionRange->SetSphereRadius(400.0f);
	DetectionRange->SetCollisionEnabled(ECollisionEnabled::QueryOnly);  // 오버랩 감지만 가능

	// HP 바 위젯 컴포넌트
	HealthBarComponent = CreateDefaultSubobject<UWidgetComponent>(TEXT("HealthBar"));
	HealthBarComponent->SetupAttachment(RootComponent);
	HealthBarComponent->SetRelativeLocation(FVector(0.0f, 0.0f, 250.0f));
	HealthBarComponent->SetWidgetSpace(EWidgetSpace::World);
	HealthBarComponent->SetDrawSize(FVector2D(120.0f, 12.0f));
	HealthBarComponent->SetWidgetClass(UAOSHealthBarWidget::StaticClass());

	CurrentHealth = MaxHealth;
}

void AAOSStructure::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AAOSStructure, CurrentHealth);
}

void AAOSStructure::OnRep_CurrentHealth()
{
	UpdateHealthBar();
}

void AAOSStructure::Multicast_OnDestroyed_Implementation()
{
	if (MeshComponent)
	{
		MeshComponent->SetVisibility(false);
	}
	if (HealthBarComponent)
	{
		HealthBarComponent->SetVisibility(false);
	}
}

void AAOSStructure::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	HealthBarWidget = nullptr;
	CurrentTarget = nullptr;
	Super::EndPlay(EndPlayReason);
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

		// HP 바 빌보드: 항상 카메라 정면을 바라봄
		if (HealthBarComponent && HealthBarComponent->IsVisible())
		{
			if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
			{
				FVector CamLoc;
				FRotator CamRot;
				PC->GetPlayerViewPoint(CamLoc, CamRot);
				FVector CamForward = CamRot.Vector();
				HealthBarComponent->SetWorldRotation((-CamForward).Rotation());
			}
		}
	}

	// 🟢 NEW - 공격 범위 디버그 시각화 (AOS.Debug.ShowAttackRange CVar)
	if (GetWorld())
	{
		IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("AOS.Debug.ShowAttackRange"));
		if (CVar && CVar->GetInt())
		{
			FColor TeamColor = (OwnerTeam == EAOSTeam::Team1) ? FColor::Blue : FColor::Red;
			FVector Pos = GetActorLocation();

			// 공격 범위 (AttackRange) — 수평 원 (XY 평면, 탑뷰에서 정확히 원형으로 보임)
			DrawDebugCircle(GetWorld(), Pos, AttackRange, 32,
				TeamColor, false, 0.0f, 0, 3.0f,
				FVector(1, 0, 0), FVector(0, 1, 0), false);

			// 감지 범위 (DetectionRange) — 밝은 혼합 색, 수평 원
			if (DetectionRange)
			{
				FColor DetectionColor = FColor(
					TeamColor.R / 2 + 128,
					TeamColor.G / 2 + 128,
					TeamColor.B / 2 + 128);
				DrawDebugCircle(GetWorld(), Pos, DetectionRange->GetUnscaledSphereRadius(), 32,
					DetectionColor, false, 0.0f, 0, 3.0f,
					FVector(1, 0, 0), FVector(0, 1, 0), false);
			}
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

	InitializeHealthBar();
}

void AAOSStructure::InitializeHealthBar()
{
	if (!HealthBarComponent)
	{
		return;
	}

	// 위젯 클래스가 설정되어 있으면 WidgetComponent에 할당
	if (HealthBarWidgetClass)
	{
		HealthBarComponent->SetWidgetClass(HealthBarWidgetClass);
	}

	HealthBarWidget = Cast<UAOSHealthBarWidget>(HealthBarComponent->GetUserWidgetObject());
	if (HealthBarWidget)
	{
		FLinearColor BarColor = (OwnerTeam == EAOSTeam::Team1) ? FLinearColor::Red : FLinearColor::Blue;
		HealthBarWidget->SetBarColor(BarColor);
		HealthBarWidget->UpdateHealthPercent(1.0f);
	}
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
	if (!HasAuthority())
	{
		return;
	}

	if (IsDestroyed())
	{
		return;
	}

	CurrentHealth = FMath::Max(0.0f, CurrentHealth - DamageAmount);
	UpdateHealthBar();

	if (IsDestroyed())
	{
		OnStructureDestroyed();
	}
}

void AAOSStructure::OnStructureDestroyed()
{
	FString TypeName = (StructureType == EStructureType::Tower) ? TEXT("Tower") : TEXT("CommandCenter");
	FString TeamName = (OwnerTeam == EAOSTeam::Team1) ? TEXT("Team1") : TEXT("Team2");
	FString LaneName;
	switch (Lane)
	{
		case EAOSLane::Top: LaneName = TEXT("Top"); break;
		case EAOSLane::Mid: LaneName = TEXT("Mid"); break;
		case EAOSLane::Bottom: LaneName = TEXT("Bottom"); break;
		default: LaneName = TEXT("Unknown"); break;
	}

	UE_LOG(LogTemp, Warning, TEXT("[Structure] %s %s %s destroyed at (%.0f, %.0f, %.0f)"),
		*TeamName, *LaneName, *TypeName,
		GetActorLocation().X, GetActorLocation().Y, GetActorLocation().Z);

	// 서버 전용: 감지 범위 비활성화 (더 이상 적을 공격하지 않음)
	if (DetectionRange)
	{
		DetectionRange->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	// 서버 전용: Tick/타겟 해제
	SetActorTickEnabled(false);
	CurrentTarget = nullptr;

	// 모든 클라이언트에 시각 효과 전파 (서버 자신도 포함)
	Multicast_OnDestroyed();
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
	float DetectionRadius = 400.0f;

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

void AAOSStructure::UpdateHealthBar()
{
	if (HealthBarWidget)
	{
		HealthBarWidget->UpdateHealthPercent(CurrentHealth / MaxHealth);
	}
}

void AAOSStructure::UpdateAttackTarget()
{
	// 현재 목표가 없거나 죽었다면 새로운 목표 찾기
	if (!CurrentTarget || !CurrentTarget->IsAlive())
	{
		CurrentTarget = FindNearestEnemy();
	}
}
