// 투척 무기 액터 구현 — 서버가 단계를 진행, 각 머신은 복제된 단계/시각으로 위치를 직접 계산.

#include "AOSThrownWeapon.h"
#include "AOS/AOSCharacter.h"
#include "GAS/Abilities/GA_SkillBase.h"
#include "AbilitySystemComponent.h"
#include "AnimationRuntime.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/SkeletalMeshSocket.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameStateBase.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "Particles/ParticleSystemComponent.h"
#include "TimerManager.h"

namespace
{
	const FGameplayTag& WeaponThrownTag()
	{
		static const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName("State.WeaponThrown"));
		return Tag;
	}

	// 레퍼런스 포즈 기준 소켓(없으면 같은 이름의 본) 의 컴포넌트 공간 트랜스폼.
	bool GetRefPoseSocketTransform(const USkeletalMesh* Mesh, FName Name, FTransform& OutTransform)
	{
		const FReferenceSkeleton& RefSkeleton = Mesh->GetRefSkeleton();
		if (const USkeletalMeshSocket* Socket = Mesh->FindSocket(Name))
		{
			const int32 BoneIndex = RefSkeleton.FindBoneIndex(Socket->BoneName);
			if (BoneIndex == INDEX_NONE) return false;
			OutTransform = Socket->GetSocketLocalTransform()
				* FAnimationRuntime::GetComponentSpaceTransformRefPose(RefSkeleton, BoneIndex);
			return true;
		}
		const int32 BoneIndex = RefSkeleton.FindBoneIndex(Name);
		if (BoneIndex == INDEX_NONE) return false;
		OutTransform = FAnimationRuntime::GetComponentSpaceTransformRefPose(RefSkeleton, BoneIndex);
		return true;
	}
}

AAOSThrownWeapon::AAOSThrownWeapon()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	SetReplicatingMovement(false);   // 위치는 각 머신이 Phase + 서버 시각으로 계산
	SetNetUpdateFrequency(20.0f);

	Pivot = CreateDefaultSubobject<USceneComponent>(TEXT("Pivot"));
	RootComponent = Pivot;

	WeaponMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("WeaponMesh"));
	WeaponMesh->SetupAttachment(Pivot);
	WeaponMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	WeaponMesh->SetGenerateOverlapEvents(false);
	WeaponMesh->SetVisibility(false);   // SetupWeaponVisual 성공 전까지 숨김
}

void AAOSThrownWeapon::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AAOSThrownWeapon, Phase);
	DOREPLIFETIME(AAOSThrownWeapon, OwnerCharacter);
	DOREPLIFETIME(AAOSThrownWeapon, StartLocation);
	DOREPLIFETIME(AAOSThrownWeapon, ImpactLocation);
	DOREPLIFETIME(AAOSThrownWeapon, FlightDuration);
	DOREPLIFETIME(AAOSThrownWeapon, PhaseStartServerTime);
}

// ============================================================
// 서버 — 초기화 / 단계 진행
// ============================================================

void AAOSThrownWeapon::InitThrow(AAOSCharacter* InOwner, UGA_SkillBase* InSkill, const FVector& InStart, const FVector& InImpact)
{
	OwnerCharacter = InOwner;
	OwningSkill = InSkill;
	StartLocation = InStart;
	ImpactLocation = InImpact;

	const float Distance = FVector::Dist(InStart, InImpact);
	FlightDuration = FMath::Clamp(Distance / FMath::Max(FlightSpeed, 1.0f), MinFlightTime, MaxFlightTime);

	// 손의 무기 숨김 + 기본공격/스킬 차단 (복제 loose 태그 — 클라도 같은 태그를 받아 본을 숨김)
	if (InOwner)
	{
		if (UAbilitySystemComponent* ASC = InOwner->GetAbilitySystemComponent())
		{
			ASC->AddLooseGameplayTag(WeaponThrownTag(), 1, EGameplayTagReplicationState::TagOnly);
			bOwnerTagApplied = true;
		}
	}

	Phase = EAOSThrownWeaponPhase::Flying;
	PhaseStartServerTime = GetServerTime();
}

void AAOSThrownWeapon::BeginPlay()
{
	Super::BeginPlay();

	// 클라는 초기 복제의 OnRep_Phase 가 BeginPlay 보다 먼저 와서 VisualStart(손)를 이미 잡았을 수 있다
	if (VisualPhase == EAOSThrownWeaponPhase::None)
	{
		VisualStart = StartLocation;
	}
	SetActorLocation(StartLocation);

	if (HasAuthority())
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(PhaseTimerHandle, this, &AAOSThrownWeapon::OnImpact, FlightDuration, false);
			World->GetTimerManager().SetTimer(SafetyTimerHandle, this, &AAOSThrownWeapon::FinishReturn, MaxLifetime, false);
		}
	}

	// 서버(리슨)와 초기 복제를 받은 클라 모두 첫 단계 비주얼 진입
	HandlePhaseVisuals();
}

void AAOSThrownWeapon::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(PhaseTimerHandle);
		World->GetTimerManager().ClearTimer(SafetyTimerHandle);
	}
	RemoveOwnerThrownTag();   // 어떤 경로로 사라지든 주인 손의 무기는 되돌린다
	Super::EndPlay(EndPlayReason);
}

void AAOSThrownWeapon::SetPhase(EAOSThrownWeaponPhase NewPhase)
{
	Phase = NewPhase;
	PhaseStartServerTime = GetServerTime();
	ForceNetUpdate();
	HandlePhaseVisuals();   // 리슨 서버 호스트 화면 (DS 는 내부에서 skip)
}

void AAOSThrownWeapon::OnImpact()
{
	if (Phase != EAOSThrownWeaponPhase::Flying) return;

	SetPhase(EAOSThrownWeaponPhase::Stuck);

	// 착지 순간 범위 피해 — 데미지 계산·팀 판정·부수효과는 던진 GA 한 곳에서
	if (UGA_SkillBase* Skill = OwningSkill.Get())
	{
		Skill->ApplyThrownWeaponImpact(ImpactLocation, ImpactRadius);
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(PhaseTimerHandle, this, &AAOSThrownWeapon::StartReturn, FMath::Max(StuckDuration, 0.01f), false);
	}
}

void AAOSThrownWeapon::StartReturn()
{
	if (Phase != EAOSThrownWeaponPhase::Stuck) return;
	SetPhase(EAOSThrownWeaponPhase::Returning);
}

void AAOSThrownWeapon::FinishReturn()
{
	if (!HasAuthority() || Phase == EAOSThrownWeaponPhase::Returned) return;

	RemoveOwnerThrownTag();
	SetPhase(EAOSThrownWeaponPhase::Returned);

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(PhaseTimerHandle);
		World->GetTimerManager().ClearTimer(SafetyTimerHandle);
	}
	// Returned 단계가 클라에 복제될 시간을 두고 파괴
	SetLifeSpan(1.0f);
}

void AAOSThrownWeapon::RemoveOwnerThrownTag()
{
	if (!bOwnerTagApplied) return;
	bOwnerTagApplied = false;

	if (OwnerCharacter)
	{
		if (UAbilitySystemComponent* ASC = OwnerCharacter->GetAbilitySystemComponent())
		{
			ASC->RemoveLooseGameplayTag(WeaponThrownTag(), 1, EGameplayTagReplicationState::TagOnly);
		}
	}
}

// ============================================================
// Tick — 위치 계산 (모든 머신) + 회수 판정 (서버)
// ============================================================

void AAOSThrownWeapon::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// 주인이 죽거나 사라지면 무기도 정리 (라운드 종료 포함)
	if (HasAuthority() && (!OwnerCharacter || !OwnerCharacter->IsAlive()))
	{
		Destroy();
		return;
	}

	switch (Phase)
	{
	case EAOSThrownWeaponPhase::Flying:
	{
		const float Alpha = FlightDuration > 0.0f
			? FMath::Clamp((GetServerTime() - PhaseStartServerTime) / FlightDuration, 0.0f, 1.0f) : 1.0f;
		FVector Direction;
		const FVector Location = EvaluateFlight(Alpha, Direction);
		SetActorLocationAndRotation(Location, Direction.Rotation());
		break;
	}
	case EAOSThrownWeaponPhase::Returning:
	{
		const FVector Target = GetHandLocation();
		const FVector Current = GetActorLocation();
		const FVector ToTarget = Target - Current;
		const float Distance = ToTarget.Size();
		const float Step = ReturnSpeed * DeltaSeconds;
		if (Distance > KINDA_SMALL_NUMBER)
		{
			const FVector Direction = ToTarget / Distance;
			SetActorLocationAndRotation(Current + Direction * FMath::Min(Step, Distance), Direction.Rotation());
		}
		if (HasAuthority() && FVector::Dist(GetActorLocation(), Target) <= CatchDistance)
		{
			FinishReturn();
		}
		break;
	}
	default:
		break;
	}
}

float AAOSThrownWeapon::GetServerTime() const
{
	const UWorld* World = GetWorld();
	if (!World) return 0.0f;
	if (const AGameStateBase* GS = World->GetGameState())
	{
		return GS->GetServerWorldTimeSeconds();
	}
	return World->GetTimeSeconds();
}

FVector AAOSThrownWeapon::GetHandLocation() const
{
	if (!OwnerCharacter) return GetActorLocation();

	// 렌더 머신은 실제 손 소켓, DS 는 메시 포즈가 갱신되지 않을 수 있어 몸통 위치로 근사
	if (GetNetMode() != NM_DedicatedServer)
	{
		if (const USkeletalMeshComponent* OwnerMesh = OwnerCharacter->GetMesh())
		{
			if (OwnerMesh->DoesSocketExist(OwnerCharacter->WeaponSocketName))
			{
				return OwnerMesh->GetSocketLocation(OwnerCharacter->WeaponSocketName);
			}
		}
	}
	return OwnerCharacter->GetActorLocation() + FVector(0.0f, 0.0f, 40.0f);
}

FVector AAOSThrownWeapon::EvaluateFlight(float Alpha, FVector& OutDirection) const
{
	// 직선 보간 + 포물선 높이 4H·a(1-a). 방향 = 도함수 (칼끝이 진행 방향).
	const FVector Delta = FVector(ImpactLocation) - VisualStart;
	const FVector Location = VisualStart + Delta * Alpha + FVector(0.0f, 0.0f, ArcHeight * 4.0f * Alpha * (1.0f - Alpha));
	OutDirection = (Delta + FVector(0.0f, 0.0f, ArcHeight * 4.0f * (1.0f - 2.0f * Alpha))).GetSafeNormal();
	if (OutDirection.IsNearlyZero())
	{
		OutDirection = FVector::DownVector;
	}
	return Location;
}

void AAOSThrownWeapon::ApplyStuckTransform()
{
	// 비행 마지막 방향으로 꽂되, 최소 MinStuckPitch 만큼 아래를 향하게
	FVector Direction;
	EvaluateFlight(1.0f, Direction);
	FRotator Rotation = Direction.Rotation();
	Rotation.Pitch = FMath::Min(Rotation.Pitch, -MinStuckPitch);
	const FVector TipForward = Rotation.Vector();
	SetActorLocationAndRotation(FVector(ImpactLocation) + TipForward * StuckDepth, Rotation);
}

// ============================================================
// 비주얼 (NetMode != DedicatedServer)
// ============================================================

void AAOSThrownWeapon::OnRep_Phase()
{
	HandlePhaseVisuals();
}

void AAOSThrownWeapon::HandlePhaseVisuals()
{
	// 위치 계산은 DS 에서도 필요 — Stuck 자세만 여기서 확정
	if (Phase == EAOSThrownWeaponPhase::Stuck)
	{
		ApplyStuckTransform();
	}

	if (GetNetMode() == NM_DedicatedServer || Phase == VisualPhase) return;
	const EAOSThrownWeaponPhase PrevPhase = VisualPhase;
	VisualPhase = Phase;

	if (!bVisualReady)
	{
		bVisualReady = SetupWeaponVisual();
	}

	switch (Phase)
	{
	case EAOSThrownWeaponPhase::Flying:
		// 렌더 머신의 비행 시작점 = 지금 보이는 주인 손 (복제 지연으로 잘린 앞부분은 서버 시각이 보정)
		VisualStart = (OwnerCharacter && !HasAuthority()) ? GetHandLocation() : FVector(StartLocation);
		if (FlightFX)
		{
			FlightFXComp = UGameplayStatics::SpawnEmitterAttached(FlightFX, Pivot, NAME_None,
				FVector::ZeroVector, FRotator::ZeroRotator, EAttachLocation::KeepRelativeOffset, /*bAutoDestroy=*/false);
		}
		if (FlightTrailFX && bVisualReady)
		{
			FlightTrailComp = UGameplayStatics::SpawnEmitterAttached(FlightTrailFX, WeaponMesh, NAME_None,
				FVector::ZeroVector, FRotator::ZeroRotator, EAttachLocation::KeepRelativeOffset, /*bAutoDestroy=*/false);
			if (FlightTrailComp)
			{
				FlightTrailComp->BeginTrails(BaseSocketName, TipSocketName, ETrailWidthMode_FromCentre, 1.0f);
			}
		}
		break;

	case EAOSThrownWeaponPhase::Stuck:
		if (FlightTrailComp)
		{
			FlightTrailComp->EndTrails();
		}
		StopFX(FlightFXComp);
		if (ImpactFX)
		{
			UGameplayStatics::SpawnEmitterAtLocation(this, ImpactFX, ImpactLocation);
		}
		if (StuckFX && bVisualReady)
		{
			StuckFXComp = UGameplayStatics::SpawnEmitterAttached(StuckFX, WeaponMesh, NAME_None,
				FVector::ZeroVector, FRotator::ZeroRotator, EAttachLocation::KeepRelativeOffset, /*bAutoDestroy=*/false);
		}
		break;

	case EAOSThrownWeaponPhase::Returning:
		if (PrevPhase == EAOSThrownWeaponPhase::Flying)
		{
			ApplyStuckTransform();   // Stuck 복제를 건너뛴 클라 — 착지점에서 출발
		}
		StopFX(StuckFXComp);
		// 귀환 중에도 칼날 궤적 (FlightTrailFX 재사용)
		if (FlightTrailFX && bVisualReady)
		{
			if (FlightTrailComp)
			{
				FlightTrailComp->EndTrails();
			}
			FlightTrailComp = UGameplayStatics::SpawnEmitterAttached(FlightTrailFX, WeaponMesh, NAME_None,
				FVector::ZeroVector, FRotator::ZeroRotator, EAttachLocation::KeepRelativeOffset, /*bAutoDestroy=*/false);
			if (FlightTrailComp)
			{
				FlightTrailComp->BeginTrails(BaseSocketName, TipSocketName, ETrailWidthMode_FromCentre, 1.0f);
			}
		}
		if (ReturnFX)
		{
			ReturnFXComp = UGameplayStatics::SpawnEmitterAttached(ReturnFX, Pivot, NAME_None,
				FVector::ZeroVector, FRotator::ZeroRotator, EAttachLocation::KeepRelativeOffset, /*bAutoDestroy=*/false);
		}
		break;

	case EAOSThrownWeaponPhase::Returned:
	{
		StopFX(FlightFXComp);
		StopFX(StuckFXComp);
		StopFX(ReturnFXComp);
		if (FlightTrailComp)
		{
			FlightTrailComp->EndTrails();
		}
		WeaponMesh->SetVisibility(false);

		if (!OwnerCharacter || !OwnerCharacter->IsAlive()) break;

		if (CatchFX)
		{
			if (USkeletalMeshComponent* OwnerMesh = OwnerCharacter->GetMesh())
			{
				UGameplayStatics::SpawnEmitterAttached(CatchFX, OwnerMesh, OwnerCharacter->WeaponSocketName);
			}
		}
		// 받기 몽타주 — 다른 몽타주(공격/스킬)를 끊지 않고, 이동 중 미끄러짐을 피하려 서 있을 때만
		if (CatchMontage && OwnerCharacter->GetMesh())
		{
			UAnimInstance* AnimInstance = OwnerCharacter->GetMesh()->GetAnimInstance();
			const bool bStanding = OwnerCharacter->GetVelocity().Size2D() < 50.0f;
			if (AnimInstance && bStanding && !AnimInstance->IsAnyMontagePlaying())
			{
				AnimInstance->Montage_Play(CatchMontage, 1.0f, EMontagePlayReturnType::MontageLength,
					FMath::Min(CatchMontageStartTime, CatchMontage->GetPlayLength()));
			}
		}
		break;
	}
	default:
		break;
	}
}

bool AAOSThrownWeapon::SetupWeaponVisual()
{
	if (!OwnerCharacter || !OwnerCharacter->GetMesh()) return false;
	USkeletalMesh* Mesh = OwnerCharacter->GetMesh()->GetSkeletalMeshAsset();
	if (!Mesh) return false;

	WeaponMesh->SetSkeletalMeshAsset(Mesh);

	// 무기 슬롯만 남기고 모든 LOD 의 다른 섹션 숨김 (SectionIndex=INDEX_NONE → 머티리얼 인덱스 직접 지정)
	const int32 WeaponMaterialIndex = WeaponMesh->GetMaterialIndex(WeaponMaterialSlotName);
	if (WeaponMaterialIndex == INDEX_NONE)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ThrownWeapon] %s: 머티리얼 슬롯 '%s' 없음 — 무기 메시 숨김"),
			*GetName(), *WeaponMaterialSlotName.ToString());
		return false;
	}
	const int32 NumMaterials = Mesh->GetMaterials().Num();
	for (int32 LODIndex = 0; LODIndex < Mesh->GetLODNum(); ++LODIndex)
	{
		for (int32 MaterialIndex = 0; MaterialIndex < NumMaterials; ++MaterialIndex)
		{
			WeaponMesh->ShowMaterialSection(MaterialIndex, INDEX_NONE, MaterialIndex == WeaponMaterialIndex, LODIndex);
		}
	}

	// 정렬: 칼끝 = Pivot 원점, 칼자루→칼끝 = +X (Pivot 회전 = 진행 방향이면 칼끝이 앞장선다)
	FTransform TipTransform, BaseTransform;
	if (!GetRefPoseSocketTransform(Mesh, TipSocketName, TipTransform) ||
		!GetRefPoseSocketTransform(Mesh, BaseSocketName, BaseTransform))
	{
		UE_LOG(LogTemp, Warning, TEXT("[ThrownWeapon] %s: 소켓 '%s'/'%s' 없음 — 무기 메시 숨김"),
			*GetName(), *TipSocketName.ToString(), *BaseSocketName.ToString());
		return false;
	}
	const FVector Tip = TipTransform.GetLocation();
	const FVector BladeDirection = (Tip - BaseTransform.GetLocation()).GetSafeNormal();
	const FQuat Align = FRotationMatrix::MakeFromX(BladeDirection).ToQuat().Inverse();
	WeaponMesh->SetRelativeLocationAndRotation(-Align.RotateVector(Tip), Align);
	WeaponMesh->SetVisibility(true);
	return true;
}

void AAOSThrownWeapon::StopFX(TObjectPtr<UParticleSystemComponent>& FX)
{
	if (FX)
	{
		FX->DeactivateSystem();
		FX = nullptr;
	}
}
