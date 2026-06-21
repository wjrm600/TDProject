#include "AOSCharacter.h"
#include "AOSAIController.h"
#include "AOSMapManager.h"
#include "AIController.h"
#include "BrainComponent.h"
#include "GAS/AOSAbilitySystemComponent.h"
#include "GAS/AOSAttributeSet.h"
#include "GAS/Abilities/GA_Attack.h"
#include "GAS/Data/AOSAttributeInitData.h"
#include "GAS/Effects/GE_Damage.h"
#include "GAS/Effects/GE_HitReact_State.h"
#include "GAS/Effects/GE_Rooted.h"
#include "UI/AOSHealthBarWidget.h"
#include "UI/AOSDamageNumberWidget.h"
#include "Animation/AnimMontage.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/WidgetComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/DataTable.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "GameplayEffect.h"
#include "GameplayEffectTypes.h"
#include "Abilities/GameplayAbility.h"
#include "DrawDebugHelpers.h"
#include "HAL/IConsoleManager.h"

AAOSCharacter::AAOSCharacter()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;

	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	GetCharacterMovement()->bOrientRotationToMovement = true;
	// MaxWalkSpeed 는 BP CharacterMovement override 를 존중 (생성자 강제 할당 금지)
	// AttributeSet 의 MoveSpeed 가 진짜 소스 (InitializeAbilitySystem 에서 동기화)
	GetCharacterMovement()->MaxAcceleration = 2048.0f;

	// AI 컨트롤러 자동 할당
	AIControllerClass = AAOSAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	// HP 바 위젯 컴포넌트 (World Space — AI 캐릭터에 Screen Space는 로컬 PC 없이 렌더 불가)
	HealthBarComponent = CreateDefaultSubobject<UWidgetComponent>(TEXT("HealthBar"));
	HealthBarComponent->SetupAttachment(RootComponent);
	HealthBarComponent->SetRelativeLocation(FVector(0.0f, 0.0f, 120.0f));
	HealthBarComponent->SetWidgetSpace(EWidgetSpace::World);
	HealthBarComponent->SetDrawSize(FVector2D(150.0f, 15.0f));
	HealthBarComponent->SetWidgetClass(UAOSHealthBarWidget::StaticClass());
	HealthBarComponent->SetTwoSided(true);  // 뒤에서 봐도 렌더 + 깜빡임 완화
	HealthBarComponent->SetBlendMode(EWidgetBlendMode::Masked);  // 알파 테스트 → 반투명 정렬 깜빡임 제거

	// --- Weapon: 무기 메시 컴포넌트 (코스메틱, 손 소켓 부착) ---
	// 생성자에선 기본 소켓("weapon_r")으로 attach 시도 — 실제 부착은 RefreshWeaponMesh 에서 재평가.
	WeaponMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WeaponMesh"));
	WeaponMeshComponent->SetupAttachment(GetMesh(), WeaponSocketName);
	WeaponMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);  // 캐릭터 통과 원칙 — 무기는 비주얼 전용
	WeaponMeshComponent->SetGenerateOverlapEvents(false);
	WeaponMeshComponent->SetVisibility(false);  // 메시 장착 전까지 숨김

	// --- GAS Phase 1: ASC + AttributeSet 부착 ---
	AbilitySystemComponent = CreateDefaultSubobject<UAOSAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

	AttributeSet = CreateDefaultSubobject<UAOSAttributeSet>(TEXT("AttributeSet"));

	// Phase 2: 데미지 GE 기본값 (BP 에서 override 가능)
	DamageGameplayEffect = UGE_Damage::StaticClass();

	// Phase 3: 기본 공격 능력 (BP 에서 추가 능력 부여 가능)
	StartupAbilities.Add(UGA_Attack::StaticClass());
}

UAbilitySystemComponent* AAOSCharacter::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

void AAOSCharacter::InitializeAbilitySystem()
{
	if (!AbilitySystemComponent)
	{
		return;
	}

	// AI 캐릭터 패턴: Owner=self, Avatar=self
	AbilitySystemComponent->InitAbilityActorInfo(this, this);

	// Phase 5+: AttributeSet 시드 — 우선순위:
	//   1) AttributeInitTable (DT) 의 row → 디자이너 친화적 중앙 데이터
	//   2) float 멤버 (MaxHealth/AttackDamage/AttackRange/AttackCooldown/MovementSpeed) → 기존 BP fallback
	//   3) MoveSpeed 는 BP CharacterMovement->MaxWalkSpeed override 우선 (BP CharacterMovement 의 값 존중)
	if (HasAuthority() && AttributeSet)
	{
		const FAOSAttributeInitRow* Row = nullptr;
		FAOSAttributeInitRow LoadedRow;

		if (UDataTable* Table = AttributeInitTable.LoadSynchronous())
		{
			static const FString CtxStr(TEXT("AAOSCharacter::InitializeAbilitySystem"));
			if (FAOSAttributeInitRow* Found = Table->FindRow<FAOSAttributeInitRow>(AttributeInitRowName, CtxStr, /*bWarnIfRowMissing=*/true))
			{
				LoadedRow = *Found;
				Row = &LoadedRow;
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("[GAS] %s: AttributeInitTable=%s 에서 row='%s' 미발견 — float 멤버 fallback 사용"),
					*GetName(), *Table->GetName(), *AttributeInitRowName.ToString());
			}
		}

		// MoveSpeed 시드: BP CharacterMovement->MaxWalkSpeed override > DT > MovementSpeed 멤버
		const float CMOverride = (GetCharacterMovement() && GetCharacterMovement()->MaxWalkSpeed > 0.0f)
			? GetCharacterMovement()->MaxWalkSpeed : 0.0f;

		const float SeedHealth      = Row ? Row->Health      : MaxHealth;
		const float SeedMaxHealth   = Row ? Row->MaxHealth   : MaxHealth;
		const float SeedAttackPower = Row ? Row->AttackPower : AttackDamage;
		const float SeedAttackRange = Row ? Row->AttackRange : AttackRange;
		const float SeedAttackSpeed = Row ? Row->AttackSpeed
			: (1.0f / FMath::Max(AttackCooldown, KINDA_SMALL_NUMBER));
		const float SeedMoveSpeed   = (CMOverride > 0.0f)
			? CMOverride
			: (Row ? Row->MoveSpeed : MovementSpeed);

		AttributeSet->InitHealth(SeedHealth);
		AttributeSet->InitMaxHealth(SeedMaxHealth);
		AttributeSet->InitAttackPower(SeedAttackPower);
		AttributeSet->InitAttackRange(SeedAttackRange);
		AttributeSet->InitAttackSpeed(SeedAttackSpeed);
		AttributeSet->InitMoveSpeed(SeedMoveSpeed);
		AttributeSet->InitDamage(0.0f);

		UE_LOG(LogTemp, Log, TEXT("[GAS] %s: ASC initialized (%s) — Health=%.0f/%.0f AP=%.0f MS=%.0f"),
			*GetName(),
			Row ? TEXT("DT") : TEXT("fallback"),
			AttributeSet->GetHealth(), AttributeSet->GetMaxHealth(),
			AttributeSet->GetAttackPower(), AttributeSet->GetMoveSpeed());
	}

	// Phase 2: Health 변경 콜백 등록 — 서버/클라이언트 모두에서 HP 바 자동 갱신
	if (AttributeSet)
	{
		AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(
			UAOSAttributeSet::GetHealthAttribute()
		).AddUObject(this, &AAOSCharacter::OnHealthAttributeChanged);

		AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(
			UAOSAttributeSet::GetMaxHealthAttribute()
		).AddUObject(this, &AAOSCharacter::OnHealthAttributeChanged);

		// Phase 3: MoveSpeed 변경 콜백 등록 → CharacterMovement->MaxWalkSpeed 동기화
		AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(
			UAOSAttributeSet::GetMoveSpeedAttribute()
		).AddUObject(this, &AAOSCharacter::OnMoveSpeedAttributeChanged);

		// 초기값 즉시 반영
		if (UCharacterMovementComponent* CM = GetCharacterMovement())
		{
			CM->MaxWalkSpeed = AttributeSet->GetMoveSpeed();
		}
	}
}

void AAOSCharacter::GiveStartupAbilities()
{
	if (!HasAuthority() || !AbilitySystemComponent)
	{
		return;
	}

	for (const TSubclassOf<UGameplayAbility>& AbilityClass : StartupAbilities)
	{
		if (!AbilityClass) continue;
		// Level=1, InputID=INDEX_NONE (AI 캐릭터는 입력 매핑 없음), SourceObject=this
		AbilitySystemComponent->GiveAbility(
			FGameplayAbilitySpec(AbilityClass, 1, INDEX_NONE, this));
	}
}

void AAOSCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	// 서버: 컨트롤러 빙의 후 ASC ActorInfo 초기화 + 능력 부여
	InitializeAbilitySystem();
	GiveStartupAbilities();
}

void AAOSCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	// Phase 2: CurrentHealth 멤버 제거 — Health 는 AttributeSet 가 ReplicatedUsing 처리
	// HP 바 팀 색상용 — DS 클라가 팀을 알아야 함
	DOREPLIFETIME(AAOSCharacter, Team);
	// 무기 비주얼 — DS 클라가 손에 든 무기를 그려야 함
	DOREPLIFETIME(AAOSCharacter, EquippedWeaponMesh);
}

void AAOSCharacter::OnRep_Team()
{
	// 클라: 팀 도착 → HP 바 색상 갱신 (위젯이 아직 없으면 Tick 이 lazy 적용)
	bHealthBarColorApplied = false;
	RefreshHealthBarTeamColor();
}

void AAOSCharacter::RefreshHealthBarTeamColor()
{
	// 위젯은 렌더링 머신(클라/리슨서버)에만 존재 — DS 서버에서는 GetUserWidgetObject()=null → skip
	if (!HealthBarComponent)
	{
		return;
	}
	if (!HealthBarWidget)
	{
		HealthBarWidget = Cast<UAOSHealthBarWidget>(HealthBarComponent->GetUserWidgetObject());
	}
	if (HealthBarWidget)
	{
		const FLinearColor BarColor = (Team == EAOSTeam::Team1) ? FLinearColor::Red : FLinearColor::Blue;
		HealthBarWidget->SetBarColor(BarColor);
		bHealthBarColorApplied = true;
	}
}

void AAOSCharacter::OnHealthAttributeChanged(const FOnAttributeChangeData& Data)
{
	// Phase 2: Health 또는 MaxHealth 변경 → HP 바 자동 갱신 (서버/클라 양쪽)
	UpdateHealthBar();
}

void AAOSCharacter::OnMoveSpeedAttributeChanged(const FOnAttributeChangeData& Data)
{
	// Phase 3: MoveSpeed 속성 변경 → CharacterMovement->MaxWalkSpeed 동기화
	// (Phase 4 의 GA_Charge 등이 MoveSpeed 모디파이 시 즉시 반영)
	if (UCharacterMovementComponent* CM = GetCharacterMovement())
	{
		CM->MaxWalkSpeed = Data.NewValue;
	}
}

void AAOSCharacter::Multicast_OnDeath_Implementation()
{
	// DeathMontage 가 없을 때만 즉시 hide — 몽타주가 있으면 Multicast_PlayDeathMontage 가 처리
	if (!DeathMontage)
	{
		SetActorHiddenInGame(true);
	}
	if (HealthBarComponent)
	{
		HealthBarComponent->SetVisibility(false);
	}
}

void AAOSCharacter::Multicast_PlayDeathMontage_Implementation()
{
	USkeletalMeshComponent* MeshComp = GetMesh();
	UAnimInstance* AnimInst = MeshComp ? MeshComp->GetAnimInstance() : nullptr;

	// 몽타주 또는 AnimInstance 가 없으면 즉시 ragdoll 전환
	if (!DeathMontage || !AnimInst)
	{
		StartRagdoll();
		return;
	}

	// ACharacter::PlayAnimMontage 사용 — Character 의 root motion replication 통합 활용
	const float Duration = PlayAnimMontage(DeathMontage);

	if (Duration <= 0.f)
	{
		StartRagdoll();
		return;
	}

	// 몽타주 길이 후 ragdoll 전환 (각 클라이언트 로컬 타이머)
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			RagdollTimerHandle,
			this, &AAOSCharacter::StartRagdoll,
			FMath::Max(0.01f, Duration), false);
	}
}

void AAOSCharacter::StartRagdoll()
{
	USkeletalMeshComponent* MeshComp = GetMesh();
	if (!MeshComp) return;

	// PhysicsAsset 미존재 시 fallback — 메시 hide (Idle 포즈 노출 방지)
	if (!MeshComp->GetPhysicsAsset())
	{
		UE_LOG(LogTemp, Warning, TEXT("[AOSCharacter] StartRagdoll: PhysicsAsset 없음 → 메시 hide fallback. SK 자산에 PhysicsAsset 할당 권장."));
		SetActorHiddenInGame(true);
		return;
	}

	// 캡슐 충돌 비활성화 — 시체가 살아있는 캐릭터를 막지 않도록
	if (UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		Capsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	// CharacterMovement 정지 (이중 안전장치)
	if (UCharacterMovementComponent* CMC = GetCharacterMovement())
	{
		CMC->SetMovementMode(MOVE_None);
		CMC->StopMovementImmediately();
	}

	// 메시를 ragdoll 모드로 전환
	// CharacterMesh 프로파일은 Pawn ignore 라 캐릭터끼리 통과 — Ragdoll 프로파일은 World 와 충돌
	MeshComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	MeshComp->SetCollisionProfileName(TEXT("Ragdoll"));
	MeshComp->SetAllBodiesSimulatePhysics(true);
	MeshComp->SetSimulatePhysics(true);
	MeshComp->WakeAllRigidBodies();
	MeshComp->bBlendPhysics = true;
}

void AAOSCharacter::Multicast_PlayHitReact_Implementation()
{
	if (!HitReactMontage) return;
	// 사망 진행 중이면 hit react 스킵 (사망 몽타주 우선)
	if (!IsAlive()) return;

	// Rule A: 공격 중이면 hit react 스킵 — DefaultSlot 충돌 방지.
	// "Ability.Attack.Basic" 태그가 활성 = GA_Attack 의 ActivationOwnedTags 가 부여 중.
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponent())
	{
		static const FGameplayTag AttackActiveTag =
			FGameplayTag::RequestGameplayTag(FName("Ability.Attack.Basic"));
		if (ASC->HasMatchingGameplayTag(AttackActiveTag))
		{
			// 공격 진행 중 — hit react 몽타주 재생 무시
			return;
		}
	}

	// Rule B: 서버에서 State.HitReact 태그 GE 적용.
	// GE 는 ASC 리플리케이션으로 클라에 자동 전파 → 클라 측 GA_Attack 도 차단됨.
	if (HasAuthority())
	{
		ApplyHitReactStateGE();
	}

	// 서버/클라 양쪽에서 몽타주 재생
	if (UAnimInstance* AnimInst = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr)
	{
		AnimInst->Montage_Play(HitReactMontage);
	}
}

void AAOSCharacter::Multicast_ShowDamageNumber_Implementation(float DamageAmount)
{
	// DS 는 렌더 없음 → skip (SpawnDamageNumber 내부에서도 가드하지만 조기 반환).
	if (GetNetMode() == NM_DedicatedServer || DamageAmount <= 0.f) return;

	const float HalfH = GetCapsuleComponent() ? GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 88.f;
	const FVector SpawnLoc = GetActorLocation() + FVector(0.f, 0.f, HalfH + 30.f);

	// 피격당한 쪽 팀 색상으로 틴트 (HP 바와 동일 규칙: Team1=Red, Team2=Blue, 가독성 위해 밝게)
	const FLinearColor Color = (Team == EAOSTeam::Team1)
		? FLinearColor(1.0f, 0.5f, 0.5f, 1.0f)
		: FLinearColor(0.55f, 0.75f, 1.0f, 1.0f);

	UAOSDamageNumberWidget::SpawnDamageNumber(this, DamageAmount, SpawnLoc, Color);
}

void AAOSCharacter::ApplyHitReactStateGE()
{
	// 서버 전용 — HasAuthority() 는 호출자(Multicast_PlayHitReact_Implementation)에서 보장.
	UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
	if (!ASC || !HitReactMontage) return;

	FGameplayEffectContextHandle Ctx = ASC->MakeEffectContext();
	Ctx.AddSourceObject(this);

	FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(
		UGE_HitReact_State::StaticClass(), 1.0f, Ctx);
	if (Spec.IsValid())
	{
		// Duration = 몽타주 길이 — HitReact 재생 동안만 공격 차단
		Spec.Data->SetSetByCallerMagnitude(
			FGameplayTag::RequestGameplayTag(FName("Data.Duration")),
			HitReactMontage->GetPlayLength());
		ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
	}
}

void AAOSCharacter::ApplyCastRoot(float Duration)
{
	// 서버 전용 — 호출자(스킬 GA, ServerInitiated)에서 서버 권한 보장.
	UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
	if (!ASC || Duration <= 0.0f) return;

	// GE_Rooted 적용 — Duration 동안 State.Rooted 부여 (StateTree task 가 이 태그로 AI 홀드).
	FGameplayEffectContextHandle Ctx = ASC->MakeEffectContext();
	Ctx.AddSourceObject(this);
	FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(
		UGE_Rooted::StaticClass(), 1.0f, Ctx);
	if (Spec.IsValid())
	{
		Spec.Data->SetSetByCallerMagnitude(
			FGameplayTag::RequestGameplayTag(FName("Data.Duration")), Duration);
		ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
	}

	// 시전 시작 시 이동 즉시 정지 — 잔여 velocity 로 미끄러지지 않도록.
	if (UCharacterMovementComponent* CMC = GetCharacterMovement())
	{
		CMC->StopMovementImmediately();
	}
}

void AAOSCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	HealthBarWidget = nullptr;
	Super::EndPlay(EndPlayReason);
}

void AAOSCharacter::BeginPlay()
{
	Super::BeginPlay();

	SetupCharacterDefaults();

	// 무기 부착: 서버에서 기본 무기 장착 → EquippedWeaponMesh 복제 → 클라 시각화 (DS)
	if (HasAuthority() && DefaultWeaponMesh)
	{
		EquipWeapon(DefaultWeaponMesh);
	}

	// GAS Phase 1: 클라이언트 사이드 ASC 초기화
	// (서버는 PossessedBy 에서 호출, 클라이언트는 ASC 가 리플리케이션된 직후 BeginPlay 에서 호출)
	if (!HasAuthority())
	{
		InitializeAbilitySystem();
	}

	// HP 바 초기화
	if (HealthBarComponent)
	{
		HealthBarWidget = Cast<UAOSHealthBarWidget>(HealthBarComponent->GetUserWidgetObject());
		if (HealthBarWidget)
		{
			FLinearColor BarColor = (Team == EAOSTeam::Team1) ? FLinearColor::Red : FLinearColor::Blue;
			HealthBarWidget->SetBarColor(BarColor);
			HealthBarWidget->UpdateHealthPercent(1.0f);
		}
	}
}

void AAOSCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Phase 3: 쿨타임 카운터 제거 — 쿨타임은 ASC 의 "Cooldown.Attack.Basic" 태그로 관리

	// HP 바 팀 색상 lazy 적용: 위젯은 렌더링 머신에서 늦게 생성될 수 있어
	// (OnRep_Team 이 위젯보다 먼저 도착하면 색이 안 칠해짐) → 위젯 준비되면 1회 적용.
	if (!bHealthBarColorApplied && GetNetMode() != NM_DedicatedServer)
	{
		RefreshHealthBarTeamColor();
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
			// 변화량이 0.5도 이상일 때만 업데이트 → 불필요한 RenderTarget Dirty 방지
			if (!TargetRot.Equals(HealthBarComponent->GetComponentRotation(), 0.5f))
			{
				HealthBarComponent->SetWorldRotation(TargetRot);
			}
		}
	}

	// 🟢 공격 범위 디버그 시각화 (AOS.Debug.ShowAttackRange CVar) — Structure 와 동일 CVar 공유
	if (GetWorld() && GetNetMode() != NM_DedicatedServer && IsAlive())
	{
		IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("AOS.Debug.ShowAttackRange"));
		if (CVar && CVar->GetInt())
		{
			FColor TeamColor = (Team == EAOSTeam::Team1) ? FColor::Red : FColor::Blue;
			FVector Pos = GetActorLocation();
			Pos.Z -= 80.0f; // 캐릭터 발 근처에 그리기

			// AttributeSet 의 AttackRange 가 단일 진실 공급원
			const float DebugAttackRange = AttributeSet
				? AttributeSet->GetAttackRange()
				: AttackRange;

			DrawDebugCircle(GetWorld(), Pos, DebugAttackRange, 32,
				TeamColor, false, 0.0f, 0, 2.0f,
				FVector(1, 0, 0), FVector(0, 1, 0), false);
			DrawDebugString(GetWorld(), Pos + FVector(DebugAttackRange, 0, 30.0f),
				FString::Printf(TEXT("[캐릭터] 공격 %.0f"), DebugAttackRange),
				nullptr, TeamColor, 0.0f, true, 1.0f);
		}
	}
}

void AAOSCharacter::SetTeam(EAOSTeam NewTeam)
{
	Team = NewTeam;

	// 팀 설정 후 HP 바 색상 즉시 반영
	// (BeginPlay 시점에는 Team이 기본값이므로 SetTeam 호출 시 업데이트 필요)
	if (HealthBarWidget)
	{
		FLinearColor BarColor = (Team == EAOSTeam::Team1) ? FLinearColor::Red : FLinearColor::Blue;
		HealthBarWidget->SetBarColor(BarColor);
		UE_LOG(LogTemp, Warning, TEXT("[Character] Team 설정 → HP 바 색상 업데이트 (Team%d)"),
			(Team == EAOSTeam::Team1) ? 1 : 2);
	}
}

void AAOSCharacter::SetLane(EAOSLane NewLane)
{
	AssignedLane = NewLane;
}

void AAOSCharacter::DeployToLane()
{
	// AI 컨트롤러에 라인 정보 전달하여 배포 시작
	// OnPossess에서 이미 호출되므로 컨트롤러는 항상 유효함
	if (AAOSAIController* AIController = Cast<AAOSAIController>(GetController()))
	{
		AIController->StartDeployment(AssignedLane);
	}
}

bool AAOSCharacter::IsAlive() const
{
	// Phase 2: AttributeSet wrapper
	return AttributeSet ? AttributeSet->GetHealth() > 0.0f : false;
}

void AAOSCharacter::ReceiveDamage(float DamageAmount)
{
	// Phase 2: GE_Damage 적용 (deprecated wrapper — Phase 3 에서 GA_Attack 으로 일원화 예정)
	if (!HasAuthority() || !AbilitySystemComponent || !DamageGameplayEffect)
	{
		return;
	}

	if (!IsAlive() || DamageAmount <= 0.0f)
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
	// Health 차감 + 사망 처리는 AttributeSet::PostGameplayEffectExecute 가 담당
}

void AAOSCharacter::OnCharacterDeath()
{
	FString TeamName = (Team == EAOSTeam::Team1) ? TEXT("Team1") : TEXT("Team2");
	FString LaneName;
	switch (AssignedLane)
	{
		case EAOSLane::Top: LaneName = TEXT("Top"); break;
		case EAOSLane::Mid: LaneName = TEXT("Mid"); break;
		case EAOSLane::Bottom: LaneName = TEXT("Bottom"); break;
		default: LaneName = TEXT("Unknown"); break;
	}

	UE_LOG(LogTemp, Warning, TEXT("[Character] %s %s lane character died at (%.0f, %.0f, %.0f)"),
		*TeamName, *LaneName,
		GetActorLocation().X, GetActorLocation().Y, GetActorLocation().Z);

	// 사망 즉시 AI Brain 정지 — StateTree 가 계속 tick 하면서 SendAttackEvent 재트리거하는 것 방지.
	// (이게 없으면 다음 tick 에 GA_Attack 활성화 → AttackMontage 가 DeathMontage 를 같은 슬롯에서 덮어쓰고,
	//  서버는 Attack 모션, 클라는 Death 모션 → 클라가 서버 위치로 보정되며 순간이동 발생)
	if (AAIController* AIC = Cast<AAIController>(GetController()))
	{
		if (UBrainComponent* Brain = AIC->GetBrainComponent())
		{
			Brain->StopLogic(TEXT("Character died"));
		}
	}

	// 사망 시 캡슐 collision 은 그대로 유지 (시체가 살아있는 캐릭터를 막는 건 게임 특색으로 의도).
	// StartRagdoll 시점에 메시가 Ragdoll 프로파일로 전환되어 ragdoll 시뮬레이션 시작.

	// Root motion 이 NavMesh 구속 없이 적용되도록 MOVE_Walking 강제.
	// AI 캐릭터는 보통 MOVE_NavWalking — backward 이동이 NavMesh 밖으로 나가면
	// snap-to-navmesh 가 작동해서 액터가 원위치로 끌려옴 → 시각적으로 안 움직임.
	if (UCharacterMovementComponent* CMC = GetCharacterMovement())
	{
		CMC->SetMovementMode(MOVE_Walking);
	}

	// 모든 클라이언트에 시각 효과 전파 (서버 자신도 포함)
	Multicast_OnDeath();

	// 서버 전용: GameMode 알림
	if (AAOSGameMode* GameMode = Cast<AAOSGameMode>(GetWorld()->GetAuthGameMode()))
	{
		GameMode->OnCharacterDestroyed(this);
	}

	// 사망 몽타주 길이 + ragdoll 정착 시간 후 destroy
	// 몽타주 없으면 RagdollSettleDuration 만 (StartRagdoll 즉시 호출됨)
	float DestroyDelay = RagdollSettleDuration;
	if (DeathMontage)
	{
		DestroyDelay = DeathMontage->GetPlayLength() + RagdollSettleDuration;
	}

	// 서버 전용: DestroyDelay 후 액터 제거 (bReplicates=true → Destroy()가 클라이언트에도 전파)
	SetLifeSpan(DestroyDelay);

	// 모든 클라에 사망 몽타주 재생 (nullptr-safe — 없으면 내부에서 early return)
	Multicast_PlayDeathMontage();
}

float AAOSCharacter::GetCurrentHealth() const
{
	// Phase 2: AttributeSet wrapper
	return AttributeSet ? AttributeSet->GetHealth() : 0.0f;
}

float AAOSCharacter::GetMaxHealth() const
{
	// Phase 2: AttributeSet wrapper (MaxHealth 멤버는 초기값 시드로만 유지)
	return AttributeSet ? AttributeSet->GetMaxHealth() : MaxHealth;
}

float AAOSCharacter::GetAttackDamage() const
{
	// Phase 3: AttributeSet wrapper (AttackDamage 멤버는 초기값 시드로만 유지)
	return AttributeSet ? AttributeSet->GetAttackPower() : AttackDamage;
}

UAnimMontage* AAOSCharacter::GetSkillMontage(FGameplayTag SkillTag) const
{
	if (!SkillTag.IsValid()) return nullptr;
	if (const TObjectPtr<UAnimMontage>* Found = SkillMontages.Find(SkillTag))
	{
		return *Found;
	}
	return nullptr;
}

FVector AAOSCharacter::GetLaneStartPosition() const
{
	if (AAOSMapManager* MapManager = Cast<AAOSMapManager>(
		UGameplayStatics::GetActorOfClass(GetWorld(), AAOSMapManager::StaticClass())))
	{
		return MapManager->GetLaneStartPosition(AssignedLane, Team);
	}
	return FVector::ZeroVector;
}

FVector AAOSCharacter::GetLaneEndPosition() const
{
	if (AAOSMapManager* MapManager = Cast<AAOSMapManager>(
		UGameplayStatics::GetActorOfClass(GetWorld(), AAOSMapManager::StaticClass())))
	{
		return MapManager->GetLaneEndPosition(AssignedLane, Team);
	}
	return FVector::ZeroVector;
}

void AAOSCharacter::SetHighlighted(bool bHighlight)
{
	if (GetMesh())
	{
		GetMesh()->SetRenderCustomDepth(bHighlight);
		GetMesh()->SetCustomDepthStencilValue(bHighlight ? 1 : 0);
	}
}

void AAOSCharacter::UpdateHealthBar()
{
	// Phase 2: AttributeSet 기반 갱신
	if (HealthBarWidget && AttributeSet)
	{
		const float Max = AttributeSet->GetMaxHealth();
		if (Max > 0.0f)
		{
			HealthBarWidget->UpdateHealthPercent(AttributeSet->GetHealth() / Max);
		}
	}
}

void AAOSCharacter::SetupCharacterDefaults()
{
	// 메시가 이미 설정되어 있으면 스킵 (블루프린트에서 설정한 경우)
	if (GetMesh()->GetSkeletalMeshAsset())
	{
		return;
	}

	// 런타임 스켈레탈 메시 로딩
	USkeletalMesh* MeshAsset = LoadObject<USkeletalMesh>(nullptr,
		TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple"));
	if (MeshAsset)
	{
		GetMesh()->SetSkeletalMeshAsset(MeshAsset);
		GetMesh()->SetRelativeLocation(FVector(0.0f, 0.0f, -90.0f));
		GetMesh()->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));
		UE_LOG(LogTemp, Warning, TEXT("[Character] 메시 설정: SKM_Manny_Simple"));
	}

	// 런타임 애니메이션 블루프린트 로딩
	// 우선순위 1: 본 프로젝트의 ABP_AOSCharacter
	UClass* AnimBPClass = LoadClass<UAnimInstance>(nullptr,
		TEXT("/Game/AOS/Anim/ABP_AOSCharacter.ABP_AOSCharacter_C"));

	// 폴백: UE5 기본 ABP_Unarmed
	if (!AnimBPClass)
	{
		AnimBPClass = LoadClass<UAnimInstance>(nullptr,
			TEXT("/Game/Characters/Mannequins/Anims/Unarmed/ABP_Unarmed.ABP_Unarmed_C"));
	}

	if (AnimBPClass)
	{
		GetMesh()->SetAnimInstanceClass(AnimBPClass);
		UE_LOG(LogTemp, Warning, TEXT("[Character] 애니메이션 설정: %s"), *AnimBPClass->GetName());
	}
}

// === Weapon Attachment (무기 부착 시스템) ===

void AAOSCharacter::EquipWeapon(UStaticMesh* WeaponMesh)
{
	// 서버 권한에서만 상태 변경 (DS) — 복제로 클라 동기화
	if (!HasAuthority())
	{
		return;
	}

	EquippedWeaponMesh = WeaponMesh;
	RefreshWeaponMesh();  // 서버(리슨서버 포함) 즉시 반영; 전용서버는 렌더 없음
}

void AAOSCharacter::OnRep_EquippedWeapon()
{
	// 클라: 복제된 무기 메시를 시각화
	RefreshWeaponMesh();
}

void AAOSCharacter::RefreshWeaponMesh()
{
	if (!WeaponMeshComponent)
	{
		return;
	}

	WeaponMeshComponent->SetStaticMesh(EquippedWeaponMesh);
	WeaponMeshComponent->SetVisibility(EquippedWeaponMesh != nullptr);

	// 손 소켓에 부착 (스켈레톤에 WeaponSocketName 소켓이 있어야 함 — 없으면 origin 유지 + 경고)
	if (USkeletalMeshComponent* MeshComp = GetMesh())
	{
		if (MeshComp->DoesSocketExist(WeaponSocketName))
		{
			WeaponMeshComponent->AttachToComponent(
				MeshComp, FAttachmentTransformRules::SnapToTargetNotIncludingScale, WeaponSocketName);
		}
		else if (EquippedWeaponMesh)
		{
			UE_LOG(LogTemp, Warning,
				TEXT("[Weapon] %s: 소켓 '%s' 가 스켈레톤에 없음 — 무기가 origin 에 부착됨. 스켈레톤에 소켓 추가 필요."),
				*GetName(), *WeaponSocketName.ToString());
		}
	}
}
