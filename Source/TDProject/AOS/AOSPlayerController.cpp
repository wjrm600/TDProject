#include "AOSPlayerController.h"
#include "AOSGameMode.h"
#include "AOSCharacter.h"
#include "Kismet/GameplayStatics.h"

AAOSPlayerController::AAOSPlayerController()
{
	bReplicates = true;
}

void AAOSPlayerController::BeginPlay()
{
	Super::BeginPlay();

	GameMode = Cast<AAOSGameMode>(GetWorld()->GetAuthGameMode());

	if (IsLocalPlayerController())
	{
		// 로컬 플레이어 초기화
		// TODO: UI 표시 (게임 준비 화면)
	}
}

void AAOSPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	if (!InputComponent)
		return;

	// TODO: 인풋 바인딩 추가
	// 캐릭터 선택, 배치 확인 등의 입력 처리
}

void AAOSPlayerController::SetCharacterDeployment(const TArray<EAOSLane>& LaneAssignments)
{
	if (LaneAssignments.Num() != 4)
	{
		UE_LOG(LogTemp, Warning, TEXT("배치는 정확히 4개의 캐릭터를 지정해야 합니다"));
		return;
	}

	CurrentDeployment = LaneAssignments;

	// 게임 모드에 배치 정보 전달
	if (GameMode)
	{
		GameMode->DeployCharacters(PlayerTeam, CurrentDeployment);
	}
}

void AAOSPlayerController::DeployCharactersToLanes()
{
	if (CurrentDeployment.Num() != 4)
	{
		UE_LOG(LogTemp, Warning, TEXT("배치 정보가 설정되지 않았습니다"));
		return;
	}

	// 플레이어의 각 캐릭터를 할당된 라인으로 배치
	for (int32 i = 0; i < PlayerCharacters.Num(); ++i)
	{
		AAOSCharacter* Char = PlayerCharacters[i];
		if (Char)
		{
			EAOSLane AssignedLane = CurrentDeployment[i];
			Char->SetLane(AssignedLane);
			Char->DeployToLane();
		}
	}
}

void AAOSPlayerController::StartGameFromPreparation()
{
	if (GameMode)
	{
		DeployCharactersToLanes();
		GameMode->StartGame();
	}
}

void AAOSPlayerController::SetPlayerTeam(EAOSTeam Team)
{
	PlayerTeam = Team;
}

// 🔴 REMOVED: SpawnPlayerCharacters() 함수는 더 이상 사용되지 않습니다.
// AOSGameMode의 SpawnCharactersAtAllSpawnPoints()로 대체되었습니다.
// 모든 캐릭터는 게임 시작 시 스폰 포인트에서 자동으로 생성됩니다.
/*
void AAOSPlayerController::SpawnPlayerCharacters()
{
	// ... 기존 구현 제거됨
}
*/
