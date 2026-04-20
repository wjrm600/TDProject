#include "AOSPlayerController.h"
#include "AOSGameMode.h"
#include "AOSGameState.h"
#include "AOSPlayerState.h"
#include "AOSCharacter.h"
#include "AOSStructure.h"
#include "UI/AOSMainMenuWidget.h"
#include "UI/AOSSettlementWidget.h"
#include "UI/AOSCharacterSelectWidget.h"
#include "UI/AOSLobbyWidget.h"
#include "Kismet/GameplayStatics.h"
#include "Camera/CameraActor.h"
#include "Engine/World.h"
#include "Blueprint/UserWidget.h"

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
		// 레벨 이름으로 메뉴/게임 레벨 판단
		FString MapName = GetWorld()->GetMapName();
		MapName.RemoveFromStart(GetWorld()->StreamingLevelsPrefix);
		bool bIsGameLevel = !MapName.Contains(TEXT("MainMenu"));

		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = this;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

		if (bIsGameLevel)
		{
			// 게임 레벨: RTS 카메라 (탑뷰)
			RTSCamera = GetWorld()->SpawnActor<ACameraActor>(
				ACameraActor::StaticClass(),
				FVector(0.0f, 0.0f, CameraHeight),
				FRotator(CameraPitch, CameraYaw, 0.0f),
				SpawnParams
			);
		}
		else
		{
			// 메인메뉴 레벨: 캐릭터를 바라보는 고정 카메라
			RTSCamera = GetWorld()->SpawnActor<ACameraActor>(
				ACameraActor::StaticClass(),
				MenuCameraLocation,
				MenuCameraRotation,
				SpawnParams
			);
		}

		if (RTSCamera)
		{
			SetViewTarget(RTSCamera);
			UE_LOG(LogTemp, Warning, TEXT("Camera created: %s"), bIsGameLevel ? TEXT("RTS") : TEXT("Menu"));
		}

		// 게임 상태 변경 델리게이트 바인딩
		// 클라이언트는 GameMode에 접근 불가 → 리플리케이트된 GameState 델리게이트 사용
		if (AAOSGameState* AOSGS = GetWorld()->GetGameState<AAOSGameState>())
		{
			AOSGS->OnGameStateChangedClient.AddDynamic(this, &AAOSPlayerController::OnGameStateChanged);
			OnGameStateChanged(AOSGS->GetCurrentState());
		}
		else if (GameMode)
		{
			GameMode->OnGameStateChanged.AddDynamic(this, &AAOSPlayerController::OnGameStateChanged);
			OnGameStateChanged(GameMode->GetAOSGameState());
		}
	}
}

void AAOSPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	if (!InputComponent)
		return;

	// 🟡 MODIFIED - RTS 카메라 4방향 이동 및 줌 입력
	InputComponent->BindAxis("MoveForward", this, &AAOSPlayerController::MoveCameraForward);
	InputComponent->BindAxis("MoveRight", this, &AAOSPlayerController::MoveCameraRight);
	InputComponent->BindAxis("CameraZoom", this, &AAOSPlayerController::ZoomCamera);
	InputComponent->BindAction("LeftMouseClick", IE_Pressed, this, &AAOSPlayerController::HandleMouseClick);

	UE_LOG(LogTemp, Warning, TEXT("Input bindings setup for RTS camera (4-directional movement + zoom)"));
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

void AAOSPlayerController::StartRound()
{
	DeployCharactersToLanes();

	// Phase 3A: 서버면 직접 StartRound, 클라이언트면 RPC로 요청
	if (HasAuthority())
	{
		if (GameMode)
		{
			GameMode->StartGame();
		}
	}
	else
	{
		Server_RequestStartRound();
	}
}

void AAOSPlayerController::SetPlayerTeam(EAOSTeam Team)
{
	PlayerTeam = Team;
}

// 🟡 MODIFIED - Tick에서 카메라 방향 기준 4방향 이동 처리
void AAOSPlayerController::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// 카메라 이동 (수평 방향 기준)
	if (RTSCamera && (CameraMoveForward != 0.0f || CameraMoveRight != 0.0f))
	{
		FVector CurrentLocation = RTSCamera->GetActorLocation();

		// 🟡 MODIFIED - Yaw(수평 회전)만 사용하여 방향 계산
		// Pitch(상하 각도)는 무시하고 수평면에서만 이동
		FRotator CameraRotation = RTSCamera->GetActorRotation();
		FRotator YawOnlyRotation(0.0f, CameraRotation.Yaw, 0.0f);

		// 수평면 기준 Forward/Right 방향 벡터
		FVector ForwardDirection = FRotationMatrix(YawOnlyRotation).GetUnitAxis(EAxis::X);
		FVector RightDirection = FRotationMatrix(YawOnlyRotation).GetUnitAxis(EAxis::Y);

		// 입력에 따라 이동
		FVector MovementDelta = FVector::ZeroVector;
		MovementDelta += ForwardDirection * CameraMoveForward * CameraMoveSpeed * DeltaTime;
		MovementDelta += RightDirection * CameraMoveRight * CameraMoveSpeed * DeltaTime;

		CurrentLocation += MovementDelta;

		// 맵 경계 체크
		CurrentLocation.X = FMath::Clamp(CurrentLocation.X, -MapBoundaryX, MapBoundaryX);
		CurrentLocation.Y = FMath::Clamp(CurrentLocation.Y, -MapBoundaryY, MapBoundaryY);

		RTSCamera->SetActorLocation(CurrentLocation);
	}
}

// 🟡 MODIFIED - 카메라 Forward/Backward 이동 (W/S 또는 Up/Down)
void AAOSPlayerController::MoveCameraForward(float AxisValue)
{
	CameraMoveForward = AxisValue;  // -1.0 (Backward) ~ 1.0 (Forward)
}

// 🟢 NEW - 카메라 Left/Right 이동 (A/D 또는 Left/Right)
void AAOSPlayerController::MoveCameraRight(float AxisValue)
{
	CameraMoveRight = AxisValue;  // -1.0 (Left) ~ 1.0 (Right)
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
		SelectedCharacter->SetHighlighted(false);
	}

	SelectedCharacter = NewCharacter;

	// 새로 선택된 캐릭터에 하이라이트 표시
	if (SelectedCharacter)
	{
		SelectedCharacter->SetHighlighted(true);
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

// 🟢 NEW - 카메라 각도 설정
void AAOSPlayerController::SetCameraAngle(float Pitch, float Yaw)
{
	CameraPitch = Pitch;
	CameraYaw = Yaw;
	if (RTSCamera)
	{
		RTSCamera->SetActorRotation(FRotator(CameraPitch, CameraYaw, 0.0f));
	}
}

// 🟢 NEW - 카메라 줌 인/아웃 (마우스 휠)
void AAOSPlayerController::ZoomCamera(float AxisValue)
{
	if (!RTSCamera || AxisValue == 0.0f)
		return;

	// 현재 높이 가져오기
	FVector CurrentLocation = RTSCamera->GetActorLocation();

	// 줌 변경량 계산 (마우스 휠 위로 = 줌 인, 아래로 = 줌 아웃)
	float ZoomDelta = -AxisValue * ZoomSpeed;

	// 새로운 높이 계산 및 범위 제한
	float NewHeight = FMath::Clamp(CurrentLocation.Z + ZoomDelta, MinZoomHeight, MaxZoomHeight);

	// 높이만 변경
	CurrentLocation.Z = NewHeight;
	RTSCamera->SetActorLocation(CurrentLocation);

	// CameraHeight 변수도 업데이트
	CameraHeight = NewHeight;
}

// 게임 상태 변경 핸들러
void AAOSPlayerController::OnGameStateChanged(EAOSGameState NewState)
{
	UE_LOG(LogTemp, Warning, TEXT("[PlayerController] 게임 상태 변경: %d"), static_cast<int32>(NewState));

	switch (NewState)
	{
	case EAOSGameState::MainMenu:
		HideLobby();
		HideSettlement();
		ShowMainMenu();
		SetInputMode(FInputModeUIOnly());
		bShowMouseCursor = true;
		break;

	case EAOSGameState::Lobby:
		HideMainMenu();
		HideSettlement();
		ShowLobby();
		SetInputMode(FInputModeUIOnly());
		bShowMouseCursor = true;
		break;

	case EAOSGameState::RoundPreparation:
		HideLobby();
		HideMainMenu();
		HideSettlement();
		ShowCharacterSelect();
		SetInputMode(FInputModeGameAndUI());
		bShowMouseCursor = true;
		break;

	case EAOSGameState::RoundRunning:
		HideLobby();
		HideMainMenu();
		HideSettlement();
		HideCharacterSelect();
		SetInputMode(FInputModeGameAndUI());
		bShowMouseCursor = true;
		break;

	case EAOSGameState::Settlement:
		HideLobby();
		HideMainMenu();
		HideCharacterSelect();
		{
			// 승리 팀 결정 — GameMode에서 확인
			EAOSTeam WinningTeam = EAOSTeam::Team1; // 기본값
			if (GameMode)
			{
				// 커맨드 센터 파괴 여부로 승리 팀 판단
				AAOSStructure* Team1Center = GameMode->GetCommandCenter(EAOSTeam::Team1);
				AAOSStructure* Team2Center = GameMode->GetCommandCenter(EAOSTeam::Team2);
				if (Team1Center && Team1Center->IsDestroyed())
				{
					WinningTeam = EAOSTeam::Team2;
				}
				else
				{
					WinningTeam = EAOSTeam::Team1;
				}
			}
			ShowSettlement(WinningTeam);
		}
		// UI 전용 입력 모드
		SetInputMode(FInputModeUIOnly());
		bShowMouseCursor = true;
		break;
	}
}

// 메인 메뉴 표시
void AAOSPlayerController::ShowMainMenu()
{
	if (!MainMenuWidget)
	{
		UClass* WidgetClass = MainMenuWidgetClass;
		if (!WidgetClass)
		{
			WidgetClass = LoadClass<UUserWidget>(nullptr, TEXT("/Game/AOS/UI/WBP_MainMenu.WBP_MainMenu_C"));
		}
		if (WidgetClass)
		{
			MainMenuWidget = CreateWidget<UAOSMainMenuWidget>(this, WidgetClass);
		}
	}

	if (MainMenuWidget && !MainMenuWidget->IsInViewport())
	{
		MainMenuWidget->AddToViewport(10);
		// UI 전용 입력 모드
		SetInputMode(FInputModeUIOnly());
		bShowMouseCursor = true;
		UE_LOG(LogTemp, Warning, TEXT("[PlayerController] 메인 메뉴 표시"));
	}
}

// 메인 메뉴 숨김
void AAOSPlayerController::HideMainMenu()
{
	if (MainMenuWidget && MainMenuWidget->IsInViewport())
	{
		MainMenuWidget->RemoveFromParent();
		UE_LOG(LogTemp, Warning, TEXT("[PlayerController] 메인 메뉴 숨김"));
	}
}

// 정산 화면 표시
void AAOSPlayerController::ShowSettlement(EAOSTeam WinningTeam)
{
	if (!SettlementWidget)
	{
		UClass* WidgetClass = SettlementWidgetClass;
		if (!WidgetClass)
		{
			WidgetClass = LoadClass<UUserWidget>(nullptr, TEXT("/Game/AOS/UI/WBP_Settlement.WBP_Settlement_C"));
		}
		if (WidgetClass)
		{
			SettlementWidget = CreateWidget<UAOSSettlementWidget>(this, WidgetClass);
		}
	}

	if (SettlementWidget)
	{
		SettlementWidget->SetResult(WinningTeam);
		if (!SettlementWidget->IsInViewport())
		{
			SettlementWidget->AddToViewport(10);
		}
		// UI 전용 입력 모드
		SetInputMode(FInputModeUIOnly());
		bShowMouseCursor = true;
		UE_LOG(LogTemp, Warning, TEXT("[PlayerController] 정산 화면 표시 (승리: Team%d)"),
			WinningTeam == EAOSTeam::Team1 ? 1 : 2);
	}
}

// 정산 화면 숨김
void AAOSPlayerController::HideSettlement()
{
	if (SettlementWidget && SettlementWidget->IsInViewport())
	{
		SettlementWidget->RemoveFromParent();
		UE_LOG(LogTemp, Warning, TEXT("[PlayerController] 정산 화면 숨김"));
	}
}

// 캐릭터 선택 UI 표시
void AAOSPlayerController::ShowCharacterSelect()
{
	if (!CharacterSelectWidget)
	{
		UClass* WidgetClass = CharacterSelectWidgetClass;
		if (!WidgetClass)
		{
			WidgetClass = LoadClass<UUserWidget>(nullptr, TEXT("/Game/AOS/UI/WBP_CharacterSelect.WBP_CharacterSelect_C"));
		}
		if (!WidgetClass)
		{
			// 위젯 블루프린트가 없으면 C++ 클래스 직접 사용
			WidgetClass = UAOSCharacterSelectWidget::StaticClass();
		}
		if (WidgetClass)
		{
			CharacterSelectWidget = CreateWidget<UAOSCharacterSelectWidget>(this, WidgetClass);
			if (CharacterSelectWidget)
			{
				CharacterSelectWidget->OnStartRoundClicked.AddDynamic(this, &AAOSPlayerController::OnStartRoundClicked);
			}
		}
	}

	if (CharacterSelectWidget)
	{
		// 기본 배치 초기화 (라인당 2명)
		LocalDeployPlan.Empty();
		LocalDeployPlan.Add(EAOSLane::Top, 2);
		LocalDeployPlan.Add(EAOSLane::Mid, 2);
		LocalDeployPlan.Add(EAOSLane::Bottom, 2);

		CharacterSelectWidget->SetLaneCount(EAOSLane::Top, 2);
		CharacterSelectWidget->SetLaneCount(EAOSLane::Mid, 2);
		CharacterSelectWidget->SetLaneCount(EAOSLane::Bottom, 2);

		if (!CharacterSelectWidget->IsInViewport())
		{
			CharacterSelectWidget->AddToViewport(10);
		}
		CharacterSelectWidget->SetVisibility(ESlateVisibility::Visible);
		UE_LOG(LogTemp, Warning, TEXT("[PlayerController] 캐릭터 선택 UI 표시"));
	}
}

// 캐릭터 선택 UI 숨김
void AAOSPlayerController::HideCharacterSelect()
{
	if (CharacterSelectWidget && CharacterSelectWidget->GetVisibility() != ESlateVisibility::Collapsed)
	{
		CharacterSelectWidget->SetVisibility(ESlateVisibility::Collapsed);
		UE_LOG(LogTemp, Warning, TEXT("[PlayerController] 캐릭터 선택 UI 숨김"));
	}
}

// 캐릭터 선택 UI에서 라운드 시작 클릭
// Phase 3A: 클라이언트는 Server RPC로 요청. 서버 권한 확인은 RPC 구현부에서 처리.
void AAOSPlayerController::OnStartRoundClicked()
{
	if (CharacterSelectWidget)
	{
		// 위젯에서 현재 라인별 배치 수 읽기
		LocalDeployPlan.Add(EAOSLane::Top, CharacterSelectWidget->GetLaneCount(EAOSLane::Top));
		LocalDeployPlan.Add(EAOSLane::Mid, CharacterSelectWidget->GetLaneCount(EAOSLane::Mid));
		LocalDeployPlan.Add(EAOSLane::Bottom, CharacterSelectWidget->GetLaneCount(EAOSLane::Bottom));

		UE_LOG(LogTemp, Warning, TEXT("[PlayerController] 라운드 시작 요청 (Top:%d, Mid:%d, Bottom:%d)"),
			LocalDeployPlan[EAOSLane::Top],
			LocalDeployPlan[EAOSLane::Mid],
			LocalDeployPlan[EAOSLane::Bottom]);
	}

	// Phase 3A: 서버 권한이면 직접 호출, 클라이언트면 RPC 사용
	// 각 라인별 배치 수를 Server RPC로 전송
	for (auto& Pair : LocalDeployPlan)
	{
		Server_SetLaneDeployCount(Pair.Key, Pair.Value);
	}

	// 준비 완료 → 서버가 양쪽 준비 확인 후 자동 라운드 시작
	Server_SetReady(true);
}

// Phase 3A: Server RPC 구현 - 라인별 배치 수 설정
bool AAOSPlayerController::Server_SetLaneDeployCount_Validate(EAOSLane Lane, int32 Count)
{
	// 라인당 0~2 범위만 허용 (MaxCharactersPerLane)
	return Count >= 0 && Count <= 2;
}

void AAOSPlayerController::Server_SetLaneDeployCount_Implementation(EAOSLane Lane, int32 Count)
{
	AAOSGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<AAOSGameMode>() : nullptr;
	AAOSPlayerState* PS = GetPlayerState<AAOSPlayerState>();

	if (GM && PS)
	{
		GM->ServerSetLaneDeployCountForPlayer(PS, Lane, Count);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[PlayerController] Server_SetLaneDeployCount: GameMode 또는 PlayerState 없음"));
	}
}

// Phase 3A: Server RPC 구현 - 준비 상태 토글
void AAOSPlayerController::Server_SetReady_Implementation(bool bReady)
{
	AAOSGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<AAOSGameMode>() : nullptr;
	AAOSPlayerState* PS = GetPlayerState<AAOSPlayerState>();

	if (GM && PS)
	{
		GM->ServerSetPlayerReady(PS, bReady);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[PlayerController] Server_SetReady: GameMode 또는 PlayerState 없음"));
	}
}

// Phase 3A: Server RPC 구현 - 라운드 시작 요청 (호스트만 허용)
void AAOSPlayerController::Server_RequestStartRound_Implementation()
{
	AAOSGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<AAOSGameMode>() : nullptr;
	if (!GM)
	{
		return;
	}

	// 양쪽 준비 확인 후 StartRound 호출 (대안: 호스트 전용으로 변경 가능)
	if (GM->AreAllPlayersReady())
	{
		GM->StartRound();
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[PlayerController] Server_RequestStartRound: 양쪽 준비 아직 안됨"));
	}
}

// 로비 화면 표시
void AAOSPlayerController::ShowLobby()
{
	if (!LobbyWidget)
	{
		UClass* WidgetClass = LobbyWidgetClass;
		if (!WidgetClass)
		{
			WidgetClass = LoadClass<UUserWidget>(nullptr, TEXT("/Game/AOS/UI/WBP_Lobby.WBP_Lobby_C"));
		}
		if (!WidgetClass)
		{
			WidgetClass = UAOSLobbyWidget::StaticClass();
		}
		if (WidgetClass)
		{
			LobbyWidget = CreateWidget<UAOSLobbyWidget>(this, WidgetClass);
		}
	}

	if (LobbyWidget)
	{
		int32 Connected = (GetWorld() && GetWorld()->GetGameState())
			? GetWorld()->GetGameState()->PlayerArray.Num() : 0;
		LobbyWidget->UpdatePlayerCount(Connected, 2);

		if (!LobbyWidget->IsInViewport())
		{
			LobbyWidget->AddToViewport(10);
		}
		UE_LOG(LogTemp, Warning, TEXT("[PlayerController] 로비 화면 표시 (%d/2)"), Connected);
	}
}

// 로비 화면 숨김
void AAOSPlayerController::HideLobby()
{
	if (LobbyWidget && LobbyWidget->IsInViewport())
	{
		LobbyWidget->RemoveFromParent();
		UE_LOG(LogTemp, Warning, TEXT("[PlayerController] 로비 화면 숨김"));
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
