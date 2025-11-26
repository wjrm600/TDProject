#include "AOSPlayerController.h"
#include "AOSGameMode.h"
#include "AOSCharacter.h"
#include "Kismet/GameplayStatics.h"
#include "Camera/CameraActor.h"
#include "Engine/World.h"

AAOSPlayerController::AAOSPlayerController()
{
	bReplicates = true;
	bShowMouseCursor = true;
	DefaultMouseCursor = EMouseCursor::Default;
}

void AAOSPlayerController::BeginPlay()
{
	Super::BeginPlay();

	GameMode = Cast<AAOSGameMode>(GetWorld()->GetAuthGameMode());

	if (IsLocalPlayerController())
	{
		// 🟢 NEW - RTS 카메라 생성 및 설정
		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = this;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

		RTSCamera = GetWorld()->SpawnActor<ACameraActor>(
			ACameraActor::StaticClass(),
			FVector(0.0f, 0.0f, CameraHeight),
			FRotator(-70.0f, 0.0f, 0.0f),
			SpawnParams
		);

		if (RTSCamera)
		{
			SetViewTarget(RTSCamera);
			UE_LOG(LogTemp, Warning, TEXT("RTS Camera created and set as view target"));
		}
	}
}

void AAOSPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	if (!InputComponent)
		return;

	// 🟢 NEW - RTS 카메라 이동 입력
	InputComponent->BindAxis("MoveForward", this, &AAOSPlayerController::MoveCamera);
	InputComponent->BindAction("LeftMouseClick", IE_Pressed, this, &AAOSPlayerController::HandleMouseClick);

	UE_LOG(LogTemp, Warning, TEXT("Input bindings setup for RTS camera"));
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

// 🟢 NEW - Tick에서 카메라 이동 처리
void AAOSPlayerController::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// 카메라 이동
	if (RTSCamera && !CameraDirection.IsZero())
	{
		FVector NewLocation = RTSCamera->GetActorLocation() + (CameraDirection * CameraMoveSpeed * DeltaTime);

		// 맵 경계 체크
		NewLocation.X = FMath::Clamp(NewLocation.X, -MapBoundaryX, MapBoundaryX);
		NewLocation.Y = FMath::Clamp(NewLocation.Y, -MapBoundaryY, MapBoundaryY);

		RTSCamera->SetActorLocation(NewLocation);
	}
}

// 🟢 NEW - 카메라 이동 입력 처리 (WASD 또는 화살표)
void AAOSPlayerController::MoveCamera(float AxisValue)
{
	// AxisValue는 -1.0 ~ 1.0 범위
	if (AxisValue != 0.0f)
	{
		// Forward/Backward 입력 처리
		CameraDirection.Y = AxisValue;  // Forward = Y축
	}
}

// 🟢 NEW - 마우스 클릭으로 캐릭터 선택
void AAOSPlayerController::HandleMouseClick()
{
	FHitResult HitResult;
	if (GetHitResultUnderCursor(ECC_Pawn, false, HitResult))
	{
		AAOSCharacter* ClickedCharacter = Cast<AAOSCharacter>(HitResult.GetActor());
		if (ClickedCharacter)
		{
			SelectCharacter(ClickedCharacter);
			UE_LOG(LogTemp, Warning, TEXT("Selected character: Team=%d, Lane=%d, Health=%.1f/%.1f"),
				static_cast<int32>(ClickedCharacter->GetTeam()),
				static_cast<int32>(ClickedCharacter->GetLane()),
				ClickedCharacter->GetCurrentHealth(),
				ClickedCharacter->GetMaxHealth());
		}
		else
		{
			// 캐릭터가 아닌 것을 클릭했으므로 선택 해제
			SelectCharacter(nullptr);
		}
	}
}

// 🟢 NEW - 캐릭터 선택 처리
void AAOSPlayerController::SelectCharacter(AAOSCharacter* NewCharacter)
{
	if (SelectedCharacter == NewCharacter)
		return;

	// 이전 선택 캐릭터 하이라이트 제거
	if (SelectedCharacter)
	{
		// TODO: 이전 선택 캐릭터의 하이라이트 제거
	}

	SelectedCharacter = NewCharacter;

	// 새로 선택된 캐릭터에 하이라이트 표시
	if (SelectedCharacter)
	{
		// TODO: 새 선택 캐릭터 하이라이트 표시
		UE_LOG(LogTemp, Warning, TEXT("Character selected"));
	}
}

// 🟢 NEW - 카메라 높이 설정
void AAOSPlayerController::SetCameraHeight(float Height)
{
	CameraHeight = Height;
	if (RTSCamera)
	{
		FVector Location = RTSCamera->GetActorLocation();
		Location.Z = Height;
		RTSCamera->SetActorLocation(Location);
	}
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
