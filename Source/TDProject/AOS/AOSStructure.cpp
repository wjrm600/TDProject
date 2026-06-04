#include "AOSStructure.h"
#include "AOSCharacter.h"
#include "AOSGameMode.h"
#include "Kismet/GameplayStatics.h"
#include "GAS/AOSAbilitySystemComponent.h"
#include "GAS/AOSAttributeSet.h"
#include "GAS/Data/AOSAttributeInitData.h"
#include "GAS/Effects/GE_Damage.h"
#include "UI/AOSHealthBarWidget.h"
#include "UI/AOSDamageNumberWidget.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Components/WidgetComponent.h"
#include "Engine/DataTable.h"
#include "EngineUtils.h"
#include "Materials/Material.h"
#include "GameFramework/PlayerController.h"
#include "Net/UnrealNetwork.h"
#include "DrawDebugHelpers.h"
#include "HAL/IConsoleManager.h"
#include "GameplayEffect.h"
#include "GameplayEffectTypes.h"
#include "AbilitySystemComponent.h"

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

	// 감지 범위 설정 — 캐릭터 도달 정지 거리(500) + AttackRange(600)보다 크게 잡아야
	// 타워가 캐릭터를 감지/추적/공격 가능 (이전: 400 → 캐릭터가 사거리 밖에서 일방적으로 타워 공격)
	DetectionRange = CreateDefaultSubobject<USphereComponent>(TEXT("DetectionRange"));
	DetectionRange->SetupAttachment(RootComponent);
	DetectionRange->SetSphereRadius(800.0f);
	DetectionRange->SetCollisionEnabled(ECollisionEnabled::QueryOnly);  // 오버랩 감지만 가능

	// HP 바 위젯 컴포넌트 (World Space)
	HealthBarComponent = CreateDefaultSubobject<UWidgetComponent>(TEXT("HealthBar"));
	HealthBarComponent->SetupAttachment(RootComponent);
	HealthBarComponent->SetRelativeLocation(FVector(0.0f, 0.0f, 250.0f));
	HealthBarComponent->SetWidgetSpace(EWidgetSpace::World);
	HealthBarComponent->SetDrawSize(FVector2D(120.0f, 12.0f));
	HealthBarComponent->SetWidgetClass(UAOSHealthBarWidget::StaticClass());
	HealthBarComponent->SetTwoSided(true);  // 뒤에서 봐도 렌더 + 깜빡임 완화
	HealthBarComponent->SetBlendMode(EWidgetBlendMode::Masked);  // 알파 테스트 → 반투명 정렬 깜빡임 제거

	// --- GAS Phase 5: ASC + AttributeSet 부착 ---
	AbilitySystemComponent = CreateDefaultSubobject<UAOSAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

	AttributeSet = CreateDefaultSubobject<UAOSAttributeSet>(TEXT("AttributeSet"));

	// 데미지 GE 기본값 (BP 에서 override 가능)
	DamageGameplayEffect = UGE_Damage::StaticClass();
}

UAbilitySystemComponent* AAOSStructure::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

void AAOSStructure::InitializeAbilitySystem()
{
	if (!AbilitySystemComponent)
	{
		return;
	}

	// 구조물 패턴: Owner=self, Avatar=self (캐릭터와 동일)
	AbilitySystemComponent->InitAbilityActorInfo(this, this);

	// Health 변경 콜백 등록 — 서버/클라이언트 모두에서 HP 바 자동 갱신
	if (AttributeSet)
	{
		AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(
			UAOSAttributeSet::GetHealthAttribute()
		).AddUObject(this, &AAOSStructure::OnHealthAttributeChanged);

		AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(
			UAOSAttributeSet::GetMaxHealthAttribute()
		).AddUObject(this, &AAOSStructure::OnHealthAttributeChanged);
	}

	// Phase 5+: 시드는 여기서 하지 않음 — StructureType 이 default(Tower) 라
	// CC 가 Tower DT 를 잘못 사용하는 문제. Initialize(Type, ...) 가 호출되며
	// StructureType 확정 후 ApplyAttributeSeeds() 가 정확한 DT 로 시드.
	// (AttributeSet 생성자의 default(Health=100) 가 임시값으로 유지되며,
	//  Initialize 까지의 짧은 시간에는 HP 바가 임시값을 표시 — 같은 프레임 내라 무시 가능)
}

void AAOSStructure::ApplyAttributeSeeds()
{
	if (!AttributeSet) return;

	// StructureType 에 맞는 DT 선택
	TSoftObjectPtr<UDataTable> SelectedTable =
		(StructureType == EStructureType::CommandCenter)
			? CommandCenterAttributeInitTable
			: TowerAttributeInitTable;

	const FAOSAttributeInitRow* Row = nullptr;
	FAOSAttributeInitRow LoadedRow;
	if (UDataTable* Table = SelectedTable.LoadSynchronous())
	{
		static const FString CtxStr(TEXT("AAOSStructure::ApplyAttributeSeeds"));
		if (FAOSAttributeInitRow* Found = Table->FindRow<FAOSAttributeInitRow>(AttributeInitRowName, CtxStr, /*bWarnIfRowMissing=*/true))
		{
			LoadedRow = *Found;
			Row = &LoadedRow;
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("[GAS] %s: AttributeInitTable=%s 에서 row='%s' 미발견 — float 멤버 fallback"),
				*GetName(), *Table->GetName(), *AttributeInitRowName.ToString());
		}
	}

	const float SeedHealth      = Row ? Row->Health      : MaxHealth;
	const float SeedMaxHealth   = Row ? Row->MaxHealth   : MaxHealth;
	// 구조물은 공격 속성도 보유 — 기존 float 멤버 fallback
	const float SeedAttackPower = Row ? Row->AttackPower : AttackDamage;
	const float SeedAttackRange = Row ? Row->AttackRange : AttackRange;
	const float SeedAttackSpeed = Row ? Row->AttackSpeed
		: (1.0f / FMath::Max(AttackCooldown, KINDA_SMALL_NUMBER));
	const float SeedMoveSpeed   = Row ? Row->MoveSpeed   : 0.0f; // 구조물은 이동 안 함

	AttributeSet->InitHealth(SeedHealth);
	AttributeSet->InitMaxHealth(SeedMaxHealth);
	AttributeSet->InitAttackPower(SeedAttackPower);
	AttributeSet->InitAttackRange(SeedAttackRange);
	AttributeSet->InitAttackSpeed(SeedAttackSpeed);
	AttributeSet->InitMoveSpeed(SeedMoveSpeed);
	AttributeSet->InitDamage(0.0f);

	// MaxHealth 멤버도 동기화 (다른 코드가 시드로 참조하는 경우 일관성 유지)
	MaxHealth = SeedMaxHealth;

	UE_LOG(LogTemp, Log, TEXT("[GAS] %s: ASC initialized (%s, %s) — Health=%.0f/%.0f"),
		*GetName(),
		(StructureType == EStructureType::CommandCenter) ? TEXT("CC") : TEXT("Tower"),
		Row ? TEXT("DT") : TEXT("fallback"),
		AttributeSet->GetHealth(), AttributeSet->GetMaxHealth());
}

void AAOSStructure::OnHealthAttributeChanged(const FOnAttributeChangeData& Data)
{
	// Phase 5: Health 또는 MaxHealth 변경 → HP 바 자동 갱신 (서버/클라 양쪽)
	UpdateHealthBar();

	// #3 클라 신뢰성: Multicast_OnDestroyed 는 호출 시점 relevancy 에 의존해 클라에 안 닿을 수 있음.
	// 반면 Health 리플리케이션은 클라까지 확실히 도달(HP바가 클라에서 깎이는 게 그 증거)하므로,
	// Health 가 0 이 되는 시점에 각 머신(서버+클라)이 스스로 시각적으로 숨긴다.
	if (AttributeSet && AttributeSet->GetHealth() <= 0.0f)
	{
		SetActorHiddenInGame(true);
		if (MeshComponent) { MeshComponent->SetVisibility(false); }
		if (HealthBarComponent) { HealthBarComponent->SetVisibility(false); }
	}
}

void AAOSStructure::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	// Phase 5: CurrentHealth 멤버 제거 — Health 는 AttributeSet 가 ReplicatedUsing 처리
	// ⚠️ REPNOTIFY_Always: 복제값이 기본값(Tower/Team1)과 같아도 초기 복제 때 OnRep 강제 발화.
	// (기본 OnChanged 면 Team1 Tower 는 값이 기본과 같아 OnRep 미발화 → 클라 메시 셋업 누락 → 안 보임)
	DOREPLIFETIME_CONDITION_NOTIFY(AAOSStructure, StructureType, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(AAOSStructure, OwnerTeam, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME(AAOSStructure, Lane);
	DOREPLIFETIME(AAOSStructure, bDestroyedVisual); // #3 파괴 시각 상태 (OnRep 이 클라 메시 숨김)
}

void AAOSStructure::OnRep_StructureType()
{
	// 클라: 타입 수신 → 메시 셋업 (서버는 Initialize 에서 이미 처리)
	if (!HasAuthority())
	{
		SetupMeshForCurrentType();
	}
}

void AAOSStructure::SetupMeshForCurrentType()
{
	// 이미 파괴된 구조물이 늦게 복제된 경우(late-join) 메시를 다시 보이게 하지 않음
	if (bDestroyedVisual)
	{
		return;
	}

	if (StructureType == EStructureType::CommandCenter)
	{
		SetupCommandCenterMesh();
	}
	else
	{
		SetupTowerMesh();
	}
}

void AAOSStructure::OnRep_DestroyedVisual()
{
	if (!bDestroyedVisual)
	{
		return;
	}

	// 액터 전체 숨김 (메시 + HP바). 클라 중복 스폰 제거 후엔 이 복제 구조물이 유일하므로 확실히 사라짐.
	SetActorHiddenInGame(true);
	if (MeshComponent) { MeshComponent->SetVisibility(false); }
	if (HealthBarComponent) { HealthBarComponent->SetVisibility(false); }
}

void AAOSStructure::OnRep_OwnerTeam()
{
	// 클라이언트에서 OwnerTeam 수신 시 HP 바 색상 갱신
	if (!HealthBarWidget && HealthBarComponent)
	{
		InitializeHealthBar();
	}
	else if (HealthBarWidget)
	{
		FLinearColor BarColor = (OwnerTeam == EAOSTeam::Team1) ? FLinearColor::Red : FLinearColor::Blue;
		HealthBarWidget->SetBarColor(BarColor);
	}

	// 클라: 메시 머티리얼 색은 OwnerTeam 에 의존 → 타입/팀 도착 순서 무관하게 올바른 색으로 재셋업.
	// (이미 파괴된 구조물이 늦게 복제되면 OnRep_DestroyedVisual 가 다시 숨김)
	if (!HasAuthority())
	{
		SetupMeshForCurrentType();
	}
}

void AAOSStructure::Multicast_OnDestroyed_Implementation()
{
	// 액터 전체를 숨김 — C++ MeshComponent 뿐 아니라 BP 에서 추가한 메시 컴포넌트까지 모두 가려짐.
	// (레벨의 타워 BP 가 C++ MeshComponent 와 별개 메시를 가질 수 있어, MeshComponent 만
	//  SetVisibility(false) 하면 BP 메시가 그대로 남는 문제 방지)
	SetActorHiddenInGame(true);

	// 명시적으로도 한 번 더 (SetActorHiddenInGame 가 처리하지만 의도 명확화)
	if (MeshComponent)
	{
		MeshComponent->SetVisibility(false);
	}
	if (HealthBarComponent)
	{
		HealthBarComponent->SetVisibility(false);
	}

	UE_LOG(LogTemp, Log, TEXT("[Structure] %s 파괴 — 메시 숨김 (SetActorHiddenInGame)"), *GetName());
}

void AAOSStructure::Multicast_ShowDamageNumber_Implementation(float DamageAmount)
{
	if (GetNetMode() == NM_DedicatedServer || DamageAmount <= 0.f) return;

	// 구조물은 높이가 제각각(타워/CC) → 바운드 상단 기준으로 머리 위 위치 산출
	FVector Origin, Extent;
	GetActorBounds(/*bOnlyCollidingComponents*/ false, Origin, Extent);
	const FVector SpawnLoc = Origin + FVector(0.f, 0.f, Extent.Z + 40.f);

	const FLinearColor Color = (OwnerTeam == EAOSTeam::Team1)
		? FLinearColor(1.0f, 0.5f, 0.5f, 1.0f)
		: FLinearColor(0.55f, 0.75f, 1.0f, 1.0f);

	UAOSDamageNumberWidget::SpawnDamageNumber(this, DamageAmount, SpawnLoc, Color);
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

	// Phase 5: ASC 초기화 (서버/클라 양쪽 — 구조물은 Pawn 이 아니라 PossessedBy 가 없음)
	InitializeAbilitySystem();

	// 클라이언트에서도 HP 바 위젯을 초기화
	// (서버는 Initialize() 내에서 InitializeHealthBar()를 호출하지만, 클라이언트는 이를 수신하지 않음)
	// OwnerTeam은 아직 리플리케이션 전일 수 있어 기본 색상으로 설정 — OnRep_OwnerTeam에서 갱신됨
	InitializeHealthBar();

	// 클라: 복제 구조물 메시 셋업 (서버는 Initialize 에서 처리).
	// ⚠️ OnRep 에 의존하면 안 됨 — UE 초기 복제는 CDO 기본값과 다른 프로퍼티만 전송하므로
	//   Team1/Tower(둘 다 기본값) 구조물은 OwnerTeam/StructureType 이 아예 전송 안 돼 OnRep 미발화.
	//   BeginPlay 시점엔 복제 프로퍼티가 이미 적용됨 + 기본값은 생성자 기본과 동일 → 항상 정확.
	if (!HasAuthority())
	{
		SetupMeshForCurrentType();
	}
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

		// HP 바 빌보드: World Space에서 카메라 정면을 향하도록 (DS에서는 스킵)
		if (GetNetMode() != NM_DedicatedServer && HealthBarComponent && HealthBarComponent->IsVisible())
		{
			if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
			{
				FVector CamLoc;
				FRotator CamRot;
				PC->GetPlayerViewPoint(CamLoc, CamRot);
				FRotator TargetRot = (-CamRot.Vector()).Rotation();
				if (!TargetRot.Equals(HealthBarComponent->GetComponentRotation(), 0.5f))
				{
					HealthBarComponent->SetWorldRotation(TargetRot);
				}
			}
		}
	}

	// 🟢 공격 범위 디버그 시각화 (AOS.Debug.ShowAttackRange CVar) — DS는 렌더 없으므로 스킵
	if (GetWorld() && GetNetMode() != NM_DedicatedServer)
	{
		IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("AOS.Debug.ShowAttackRange"));
		if (CVar && CVar->GetInt())
		{
			FColor TeamColor = (OwnerTeam == EAOSTeam::Team1) ? FColor::Blue : FColor::Red;
			FVector Pos = GetActorLocation();
			const FString TypeStr = (StructureType == EStructureType::Tower) ? TEXT("타워") : TEXT("본진");

			// 공격 범위 — AttributeSet 의 AttackRange 가 단일 진실 공급원
			// (DT 또는 GE 모디파이로 변경된 값을 그대로 시각화)
			const float DebugAttackRange = AttributeSet
				? AttributeSet->GetAttackRange()
				: AttackRange;

			DrawDebugCircle(GetWorld(), Pos, DebugAttackRange, 32,
				TeamColor, false, 0.0f, 0, 3.0f,
				FVector(1, 0, 0), FVector(0, 1, 0), false);
			DrawDebugString(GetWorld(), Pos + FVector(DebugAttackRange, 0, 50.0f),
				FString::Printf(TEXT("[%s] 공격 %.0f"), *TypeStr, DebugAttackRange),
				nullptr, TeamColor, 0.0f, true, 1.2f);

			// 감지 범위 (DetectionRange) — 밝은 혼합 색, 수평 원 + 라벨
			if (DetectionRange)
			{
				FColor DetectionColor = FColor(
					TeamColor.R / 2 + 128,
					TeamColor.G / 2 + 128,
					TeamColor.B / 2 + 128);
				const float DetRadius = DetectionRange->GetUnscaledSphereRadius();
				DrawDebugCircle(GetWorld(), Pos, DetRadius, 32,
					DetectionColor, false, 0.0f, 0, 3.0f,
					FVector(1, 0, 0), FVector(0, 1, 0), false);
				DrawDebugString(GetWorld(), Pos + FVector(DetRadius, 0, 50.0f),
					FString::Printf(TEXT("[%s] 감지 %.0f"), *TypeStr, DetRadius),
					nullptr, DetectionColor, 0.0f, true, 1.2f);
			}
		}
	}
}

void AAOSStructure::Initialize(EStructureType Type, EAOSTeam InOwnerTeam, EAOSLane InLane)
{
	StructureType = Type;
	OwnerTeam = InOwnerTeam;
	Lane = InLane;

	// 메시 설정 (체력 등 수치는 ApplyAttributeSeeds 에서 DT 또는 float 멤버 fallback 으로 결정)
	if (Type == EStructureType::CommandCenter)
	{
		// MaxHealth fallback default — DT 가 없을 때만 사용
		if (MaxHealth <= 1000.0f) MaxHealth = 5000.0f;
		SetupCommandCenterMesh();
	}
	else if (Type == EStructureType::Tower)
	{
		// MaxHealth 는 default 1000 그대로 fallback
		SetupTowerMesh();
	}

	// Phase 5+: StructureType 이 정해진 시점에 AttributeSet 재시드
	// (BeginPlay 가 먼저 default Tower 로 시드한 뒤 Initialize 가 정확한 타입으로 덮어씀)
	if (HasAuthority() && AttributeSet)
	{
		ApplyAttributeSeeds();
	}

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
	// Phase 5: GE_Damage 적용 (캐릭터 ReceiveDamage 와 동일 패턴)
	if (!HasAuthority() || !AbilitySystemComponent || !DamageGameplayEffect)
	{
		return;
	}

	if (IsDestroyed() || DamageAmount <= 0.0f)
	{
		return;
	}

	FGameplayEffectContextHandle Ctx = AbilitySystemComponent->MakeEffectContext();
	Ctx.AddSourceObject(this);

	FGameplayEffectSpecHandle Spec = AbilitySystemComponent->MakeOutgoingSpec(
		DamageGameplayEffect, 1.0f, Ctx);
	if (Spec.IsValid())
	{
		Spec.Data->SetSetByCallerMagnitude(
			FGameplayTag::RequestGameplayTag(FName("Data.Damage")), DamageAmount);
		AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
	}
	// Health 차감 + 파괴 처리는 AttributeSet::PostGameplayEffectExecute 가 담당
}

float AAOSStructure::GetCurrentHealth() const
{
	// Phase 5: AttributeSet wrapper
	return AttributeSet ? AttributeSet->GetHealth() : 0.0f;
}

float AAOSStructure::GetMaxHealth() const
{
	// Phase 5: AttributeSet wrapper (MaxHealth 멤버는 시드로만 유지)
	return AttributeSet ? AttributeSet->GetMaxHealth() : MaxHealth;
}

bool AAOSStructure::IsDestroyed() const
{
	// Phase 5: AttributeSet wrapper
	return AttributeSet ? AttributeSet->GetHealth() <= 0.0f : false;
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

	// Slice 0: 파괴한 팀에 골드 지급 (서버 권한 — OnStructureDestroyed 는 서버에서 호출됨)
	if (AAOSGameMode* GM = Cast<AAOSGameMode>(UGameplayStatics::GetGameMode(this)))
	{
		GM->OnStructureDestroyedAwardGold(this);
	}

	// #3 파괴 시각 상태 복제 — 멀티캐스트보다 신뢰성 높음(relevancy/dormancy 무관하게 전파).
	// 서버에서 직접 적용 + 복제 강제(FlushNetDormancy) + 호스트용 OnRep 수동 호출.
	bDestroyedVisual = true;
	FlushNetDormancy();
	OnRep_DestroyedVisual();

	// 모든 클라이언트에 시각 효과 전파 (서버 자신도 포함 — 향후 VFX 용)
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
		UE_LOG(LogTemp, Warning, TEXT("[Tower] %s → 캐릭터 공격! Distance=%.0f, Damage=%.0f, TargetHP=%.0f"),
			*GetName(), Distance, AttackDamage, Target->GetCurrentHealth());
	}
}

AAOSCharacter* AAOSStructure::FindNearestEnemy()
{
	float NearestDistance = FLT_MAX;
	AAOSCharacter* NearestEnemy = nullptr;
	// 하드코딩 제거 → 실제 감지 sphere component 반지름 사용 (디버그 원과 일치)
	const float DetectionRadius = DetectionRange
		? DetectionRange->GetUnscaledSphereRadius()
		: 800.0f;

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
	// Phase 5: AttributeSet 기반 갱신
	if (HealthBarWidget && AttributeSet)
	{
		const float Max = AttributeSet->GetMaxHealth();
		if (Max > 0.0f)
		{
			HealthBarWidget->UpdateHealthPercent(AttributeSet->GetHealth() / Max);
		}
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
