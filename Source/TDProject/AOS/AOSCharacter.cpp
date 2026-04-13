#include "AOSCharacter.h"
#include "AOSAIController.h"
#include "UI/AOSHealthBarWidget.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/WidgetComponent.h"
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

	// AI 컨트롤러 자동 할당
	AIControllerClass = AAOSAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	// HP 바 위젯 컴포넌트
	HealthBarComponent = CreateDefaultSubobject<UWidgetComponent>(TEXT("HealthBar"));
	HealthBarComponent->SetupAttachment(RootComponent);
	HealthBarComponent->SetRelativeLocation(FVector(0.0f, 0.0f, 120.0f));
	HealthBarComponent->SetWidgetSpace(EWidgetSpace::World);
	HealthBarComponent->SetDrawSize(FVector2D(150.0f, 15.0f));
	HealthBarComponent->SetWidgetClass(UAOSHealthBarWidget::StaticClass());
}

void AAOSCharacter::BeginPlay()
{
	Super::BeginPlay();

	SetupCharacterDefaults();
	CurrentHealth = MaxHealth;

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

	// 공격 쿨타임 업데이트
	if (CurrentAttackCooldown > 0.0f)
	{
		CurrentAttackCooldown -= DeltaTime;
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
	if (!IsAlive())
	{
		return;
	}

	CurrentHealth = FMath::Max(0.0f, CurrentHealth - DamageAmount);
	UpdateHealthBar();

	if (!IsAlive())
	{
		OnCharacterDeath();
	}
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

	GetCharacterMovement()->StopMovementImmediately();
	SetActorEnableCollision(false);
	SetActorHiddenInGame(true);
	SetActorTickEnabled(false);

	// HP 바 숨기기
	if (HealthBarComponent)
	{
		HealthBarComponent->SetVisibility(false);
	}

	// GameMode에 사망 알림
	if (AAOSGameMode* GameMode = Cast<AAOSGameMode>(GetWorld()->GetAuthGameMode()))
	{
		GameMode->OnCharacterDestroyed(this);
	}

	// 2초 후 액터 제거
	SetLifeSpan(2.0f);
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

void AAOSCharacter::UpdateHealthBar()
{
	if (HealthBarWidget)
	{
		HealthBarWidget->UpdateHealthPercent(CurrentHealth / MaxHealth);
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
	UClass* AnimBPClass = LoadClass<UAnimInstance>(nullptr,
		TEXT("/Game/Characters/Mannequins/Anims/Unarmed/ABP_Unarmed.ABP_Unarmed_C"));
	if (AnimBPClass)
	{
		GetMesh()->SetAnimInstanceClass(AnimBPClass);
		UE_LOG(LogTemp, Warning, TEXT("[Character] 애니메이션 설정: ABP_Unarmed"));
	}
}
