#include "AOSCharacter.h"
#include "AOSAIController.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"

AAOSCharacter::AAOSCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->MaxWalkSpeed = MovementSpeed;
	GetCharacterMovement()->MaxAcceleration = 2048.0f;

	CurrentHealth = MaxHealth;

	// 🟡 MODIFIED - AI 컨트롤러 자동 할당
	// AutoPossessAI = PlacedInWorld일 때 작동하려면 이렇게 설정해야 함
	AIControllerClass = AAOSAIController::StaticClass();
}

void AAOSCharacter::BeginPlay()
{
	Super::BeginPlay();

	SetupCharacterDefaults();
	CurrentHealth = MaxHealth;
}

void AAOSCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// 공격 쿨타임 업데이트
	if (CurrentAttackCooldown > 0.0f)
	{
		CurrentAttackCooldown -= DeltaTime;
	}
}

void AAOSCharacter::SetTeam(EAOSTeam NewTeam)
{
	Team = NewTeam;
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
	return CurrentHealth > 0.0f;
}

void AAOSCharacter::ReceiveDamage(float DamageAmount)
{
	CurrentHealth = FMath::Max(0.0f, CurrentHealth - DamageAmount);

	if (!IsAlive())
	{
		// 캐릭터 사망 처리
		// TODO: 사망 애니메이션, 드롭 아이템, 재스폰 등
		GetCharacterMovement()->StopMovementImmediately();
	}
}

float AAOSCharacter::GetCurrentHealth() const
{
	return CurrentHealth;
}

FVector AAOSCharacter::GetLaneStartPosition() const
{
	// TODO: 맵 레이아웃에 따라 라인의 시작 위치 반환
	// 현재는 기본값 반환, 실제로는 맵 데이터 기반으로 구현
	switch (AssignedLane)
	{
		case EAOSLane::Top:
			return Team == EAOSTeam::Team1 ? FVector(1000, 1000, 0) : FVector(-1000, -1000, 0);
		case EAOSLane::Mid:
			return Team == EAOSTeam::Team1 ? FVector(1000, 0, 0) : FVector(-1000, 0, 0);
		case EAOSLane::Bottom:
			return Team == EAOSTeam::Team1 ? FVector(1000, -1000, 0) : FVector(-1000, 1000, 0);
		default:
			return FVector::ZeroVector;
	}
}

FVector AAOSCharacter::GetLaneEndPosition() const
{
	// TODO: 맵 레이아웃에 따라 라인의 끝 위치 반환
	switch (AssignedLane)
	{
		case EAOSLane::Top:
			return Team == EAOSTeam::Team1 ? FVector(-1000, -1000, 0) : FVector(1000, 1000, 0);
		case EAOSLane::Mid:
			return Team == EAOSTeam::Team1 ? FVector(-1000, 0, 0) : FVector(1000, 0, 0);
		case EAOSLane::Bottom:
			return Team == EAOSTeam::Team1 ? FVector(-1000, 1000, 0) : FVector(1000, -1000, 0);
		default:
			return FVector::ZeroVector;
	}
}

void AAOSCharacter::SetupCharacterDefaults()
{
	// 기본 스켈레탈 메시 설정 (프로젝트 기반 메시 사용)
	// TODO: 실제 캐릭터 메시 할당
}
