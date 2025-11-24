#include "AOSSpawnPoint.h"
#include "AOSCharacter.h"
#include "Components/BillboardComponent.h"

AAOSSpawnPoint::AAOSSpawnPoint()
{
	PrimaryActorTick.bCanEverTick = false;

	// 에디터에서 보이도록 하는 빌보드 추가
	BillboardComponent = CreateDefaultSubobject<UBillboardComponent>(TEXT("BillboardComponent"));
	RootComponent = BillboardComponent;

	// 기본 값
	Team = EAOSTeam::Team1;
	Lane = EAOSLane::Mid;
	SpawnIndex = 0;
}

void AAOSSpawnPoint::BeginPlay()
{
	Super::BeginPlay();
}

void AAOSSpawnPoint::Initialize(EAOSTeam InTeam, EAOSLane InLane, int32 InSpawnIndex)
{
	Team = InTeam;
	Lane = InLane;
	SpawnIndex = InSpawnIndex;
}

void AAOSSpawnPoint::SetOccupiedCharacter(AAOSCharacter* Character)
{
	OccupiedCharacter = Character;

	// 🟡 MODIFIED - 위치 설정 제거 (InitializeCharacter에서 처리)
	// if (Character)
	// {
	//     Character->SetActorLocation(GetActorLocation());
	// }
}

void AAOSSpawnPoint::ReleaseCharacter()
{
	OccupiedCharacter = nullptr;
}

// 🟢 NEW - 스폰 포인트에서 캐릭터 생성
AAOSCharacter* AAOSSpawnPoint::SpawnCharacterAtPoint(TSubclassOf<AAOSCharacter> CharacterClass)
{
	if (!CharacterClass || !GetWorld())
	{
		return nullptr;
	}

	// 🟡 MODIFIED - SpawnParameters에서 Owner를 설정하여 AIController가 올바르게 할당되도록 함
	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = this;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	// 스폰 포인트 위치에 캐릭터 생성
	AAOSCharacter* NewCharacter = GetWorld()->SpawnActor<AAOSCharacter>(
		CharacterClass,
		GetActorLocation(),
		FRotator::ZeroRotator,
		SpawnParams
	);

	if (NewCharacter)
	{
		InitializeCharacter(NewCharacter);
	}

	return NewCharacter;
}

// 🟢 NEW - 생성된 캐릭터 초기화
void AAOSSpawnPoint::InitializeCharacter(AAOSCharacter* Character)
{
	if (!Character)
	{
		return;
	}

	// 1. 캐릭터 팀/라인 설정
	Character->SetTeam(Team);
	Character->SetLane(Lane);

	// 2. 스폰 포인트에 등록 (위치는 이미 SpawnActor에서 설정됨)
	SetOccupiedCharacter(Character);

	// 3. DeployToLane() 호출
	//    Pawn::BeginPlay() 중에 SpawnDefaultController() 호출
	//    └─ AIController 생성 및 Possess() 호출
	//       └─ OnPossess()에서 StartDeployment() 호출
	//    따라서 이 시점에서는 이미 배포가 시작됨
	Character->DeployToLane();

	UE_LOG(LogTemp, Warning, TEXT("Character spawned at SpawnPoint - Team: %d, Lane: %d, Position: (%.1f, %.1f, %.1f)"),
	       static_cast<int32>(Team), static_cast<int32>(Lane),
	       Character->GetActorLocation().X, Character->GetActorLocation().Y, Character->GetActorLocation().Z);
}

#if WITH_EDITOR
void AAOSSpawnPoint::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (PropertyChangedEvent.Property != nullptr)
	{
		const FName PropertyName = PropertyChangedEvent.Property->GetFName();

		// Team, Lane, SpawnIndex가 변경되면 에디터 표시 업데이트
		if (PropertyName == GET_MEMBER_NAME_CHECKED(AAOSSpawnPoint, Team) ||
			PropertyName == GET_MEMBER_NAME_CHECKED(AAOSSpawnPoint, Lane) ||
			PropertyName == GET_MEMBER_NAME_CHECKED(AAOSSpawnPoint, SpawnIndex))
		{
			// 아이콘 색상 변경 (Team에 따라)
			if (Team == EAOSTeam::Team1)
			{
				// Red 색상
				BillboardComponent->SetHiddenInGame(false);
			}
			else
			{
				// Blue 색상
				BillboardComponent->SetHiddenInGame(false);
			}
		}
	}
}
#endif
