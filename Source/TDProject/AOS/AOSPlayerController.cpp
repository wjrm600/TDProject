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
#include "UI/AOSMinimapWidget.h"
#include "UI/AOSBanPickWidget.h"
#include "UI/AOSCharacterPreviewStage.h"
#include "Kismet/GameplayStatics.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Engine/World.h"
#include "Blueprint/UserWidget.h"
#include "HAL/IConsoleManager.h"

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
			// 종횡비 제약 해제: bConstrainAspectRatio=true 면 레터박스(검은 띠)가 생기고
			// DrawDebugString 텍스트가 실제 게임 영역 밖에 렌더링되어 위치가 어긋남.
			// false 로 설정하면 뷰포트 크기에 맞게 수평 FOV만 조정되고 디버그 위치가 정확해짐.
			if (UCameraComponent* CamComp = RTSCamera->GetCameraComponent())
			{
				CamComp->bConstrainAspectRatio = false;
			}

			SetViewTarget(RTSCamera);
			UE_LOG(LogTemp, Warning, TEXT("Camera created: %s"), bIsGameLevel ? TEXT("RTS") : TEXT("Menu"));
		}

		// 게임 상태 변경 델리게이트 바인딩
		// 클라이언트는 GameMode에 접근 불가 → 리플리케이트된 GameState 델리게이트 사용
		if (AAOSGameState* AOSGS = GetWorld()->GetGameState<AAOSGameState>())
		{
			AOSGS->OnGameStateChangedClient.AddDynamic(this, &AAOSPlayerController::OnGameStateChanged);

			EAOSGameState InitialState = AOSGS->GetCurrentState();

			// ── 타이밍 버그 방어 ──────────────────────────────────────────────────
			// BeginPlay 실행 순서는 항상 일정하지 않다.
			// 경우 1: GameMode::BeginPlay 먼저 → TransitionToLobby → CurrentState=Lobby(1)
			//          → PC::BeginPlay 실행 시 InitialState=1 이므로 즉시 처리 ✓
			// 경우 2: PostLogin 또는 PC::BeginPlay 가 먼저 실행
			//          → GameMode::BeginPlay 아직 미실행 → CurrentState=MainMenu(0) (기본값)
			//          → 즉시 처리하면 게임 레벨에서 메인 메뉴 위젯이 잘못 표시됨 ✗
			// 경우 3: (클라이언트) 서버 리플리케이션 미도착
			//          → CurrentState=MainMenu(0) (기본값) → 즉시 처리 시 동일 문제 ✗
			//
			// 해결:
			//  · 게임 레벨에서 InitialState=MainMenu(0) 이면 기다린다.
			//    - 서버: GameMode::BeginPlay 가 TransitionToLobby → OnGameStateChangedClient 브로드캐스트
			//    - 클라이언트: OnRep_CurrentState 가 Lobby 전달
			//  · 메뉴 레벨 또는 InitialState≠0 이면 즉시 처리
			// ─────────────────────────────────────────────────────────────────────
			const bool bWaitingForLobbyState = bIsGameLevel
				&& (InitialState == EAOSGameState::MainMenu);

			if (!bWaitingForLobbyState)
			{
				UE_LOG(LogTemp, Warning, TEXT("[PlayerController] BeginPlay 초기 상태 처리: %d (IsServer=%d)"),
					(int32)InitialState, HasAuthority() ? 1 : 0);
				OnGameStateChanged(InitialState);
			}
			else
			{
				// 게임 레벨에서 기본값(0) → 아직 초기화되지 않은 상태 → 델리게이트 대기
				UE_LOG(LogTemp, Warning, TEXT("[PlayerController] BeginPlay: 게임레벨 초기화 대기 중... (IsServer=%d)"),
					HasAuthority() ? 1 : 0);
			}
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

	// ─── 디버그 치트키 ───────────────────────────────────────
	// F1: 타워/커맨드센터 박스 표시 토글
	InputComponent->BindKey(EKeys::F1, IE_Pressed, this, &AAOSPlayerController::DebugToggleStructureBoxes);
	// F2: 공격 범위 표시 토글
	InputComponent->BindKey(EKeys::F2, IE_Pressed, this, &AAOSPlayerController::DebugToggleAttackRange);
	// F3: AI 캐릭터 이동 경로 표시 토글
	InputComponent->BindKey(EKeys::F3, IE_Pressed, this, &AAOSPlayerController::DebugToggleCharacterPaths);
	// ─────────────────────────────────────────────────────────

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

	// 준비 단계 타이머 UI 갱신 (GameState 경유 — 클라이언트도 동작)
	if (CharacterSelectWidget && CharacterSelectWidget->GetVisibility() == ESlateVisibility::Visible)
	{
		if (AAOSGameState* AOSGS = GetWorld()->GetGameState<AAOSGameState>())
		{
			if (AOSGS->GetCurrentState() == EAOSGameState::RoundPreparation)
			{
				const float TimeLeft = AOSGS->PreparationTimeRemaining;
				CharacterSelectWidget->UpdatePreparationTimer(TimeLeft);

				// 3초 전 자동 배치 전송: 위젯에 캐릭터가 배치된 경우만 전송
				// (미배치 시 서버 기본값 2,2,1 유지)
				if (!bAutoSubmittedConfig && TimeLeft < 3.0f)
				{
					bAutoSubmittedConfig = true;
					if (CharacterSelectWidget->GetTotalCount() > 0)
					{
						const EAOSLane Lanes[] = { EAOSLane::Top, EAOSLane::Mid, EAOSLane::Bottom };
						for (EAOSLane Lane : Lanes)
						{
							TArray<TSubclassOf<AAOSCharacter>> Classes = CharacterSelectWidget->GetLaneClasses(Lane);
							TArray<int32> UnitIds = CharacterSelectWidget->GetLaneUnitIds(Lane);
							Server_SetLaneDeployClasses(Lane, Classes, UnitIds);
							Server_SetLaneDeployCount(Lane, Classes.Num());
						}
						UE_LOG(LogTemp, Warning, TEXT("[PlayerController] 준비 타이머 만료 임박 — 배치 자동 전송 (Total:%d)"),
							CharacterSelectWidget->GetTotalCount());
					}
				}
			}
		}
	}

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

	// 새로운 높이 계산 및 범위 제한 (MaxZoomInHeight=가장 확대된 낮은 높이, MaxZoomOutHeight=가장 축소된 높은 높이)
	float NewHeight = FMath::Clamp(CurrentLocation.Z + ZoomDelta, MaxZoomInHeight, MaxZoomOutHeight);

	// 높이만 변경
	CurrentLocation.Z = NewHeight;
	RTSCamera->SetActorLocation(CurrentLocation);

	// CameraHeight 변수도 업데이트
	CameraHeight = NewHeight;
}

// 라운드 시작 시: 로컬 클라이언트 카메라를 자기 팀 커맨드 센터로 포커스
// 회전(Yaw/Pitch)·줌(Z) 은 그대로 두고 XY 만 이동하여 CC 가 화면 중앙에 오도록 한다.
void AAOSPlayerController::FocusCameraOnOwnCommandCenter()
{
	if (!RTSCamera)
	{
		return;
	}

	// 로컬 플레이어 팀 — PlayerState 가 진실 공급원 (복제됨), 없으면 PlayerTeam 폴백
	EAOSTeam MyTeam = PlayerTeam;
	if (AAOSPlayerState* PS = GetPlayerState<AAOSPlayerState>())
	{
		MyTeam = PS->GetTeam();
	}

	// 내 팀의 커맨드 센터 찾기.
	// 클라이언트는 GameMode 가 없으므로(GetAuthGameMode=null) 복제된 구조물 액터를 직접 탐색한다.
	// (StructureType / OwnerTeam 은 복제되는 프로퍼티)
	AAOSStructure* OwnCC = nullptr;
	TArray<AActor*> Structures;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AAOSStructure::StaticClass(), Structures);
	for (AActor* A : Structures)
	{
		AAOSStructure* S = Cast<AAOSStructure>(A);
		if (S && S->GetStructureType() == EStructureType::CommandCenter && S->GetOwnerTeam() == MyTeam)
		{
			OwnCC = S;
			break;
		}
	}

	if (!OwnCC)
	{
		UE_LOG(LogTemp, Warning, TEXT("[PlayerController] FocusCameraOnOwnCommandCenter: 팀%d CC 미발견 (복제 대기?)"),
			static_cast<int32>(MyTeam));
		return;
	}

	// 현재 카메라 높이(줌)·회전을 유지한 채, 지면(z=0) 교차점이 CC 위치가 되도록 XY 재계산
	const FVector CCLoc = OwnCC->GetActorLocation();
	MoveCameraToGroundPoint(CCLoc);

	UE_LOG(LogTemp, Warning, TEXT("[PlayerController] 카메라 → 팀%d CC 포커스 (CC=%s)"),
		static_cast<int32>(MyTeam), *CCLoc.ToString());
}

// 지면(z=0) 한 점이 화면 중앙(카메라 포커스)에 오도록 RTS 카메라 XY 이동.
// 회전·줌(높이)은 유지. 클라이언트 로컬 카메라 전용(복제 불필요).
// FocusCameraOnOwnCommandCenter / 미니맵 클릭 이동이 공유.
void AAOSPlayerController::MoveCameraToGroundPoint(const FVector& GroundLocation)
{
	if (!RTSCamera)
	{
		return;
	}

	const FVector CamLoc = RTSCamera->GetActorLocation();
	const FRotator CamRot = RTSCamera->GetActorRotation();
	const FVector Fwd = CamRot.Vector(); // 단위 전방 벡터 (아래를 향하므로 Z<0)

	FVector NewLoc = CamLoc;
	if (!FMath::IsNearlyZero(Fwd.Z))
	{
		const float T = -CamLoc.Z / Fwd.Z;      // 카메라에서 지면까지의 광선 거리
		NewLoc.X = GroundLocation.X - T * Fwd.X;
		NewLoc.Y = GroundLocation.Y - T * Fwd.Y;
	}
	else
	{
		// 비정상(거의 수평) — 대상 바로 위로 폴백
		NewLoc.X = GroundLocation.X;
		NewLoc.Y = GroundLocation.Y;
	}

	// 맵 경계 클램프 (Tick 이동과 동일 규칙)
	NewLoc.X = FMath::Clamp(NewLoc.X, -MapBoundaryX, MapBoundaryX);
	NewLoc.Y = FMath::Clamp(NewLoc.Y, -MapBoundaryY, MapBoundaryY);

	RTSCamera->SetActorLocation(NewLoc);
}

// 미니맵 연동: 현재 카메라 yaw (RTSCamera 우선, 없으면 멤버 폴백)
float AAOSPlayerController::GetCameraYaw() const
{
	if (RTSCamera)
	{
		return RTSCamera->GetActorRotation().Yaw;
	}
	return CameraYaw;
}

// Slice 1: 미니맵 표시 (로컬 컨트롤러 전용). RoundRunning 진입 시 호출.
void AAOSPlayerController::ShowMinimap()
{
	if (!IsLocalPlayerController()) return;

	if (!MinimapWidget)
	{
		UClass* WidgetClass = MinimapWidgetClass;
		if (!WidgetClass)
		{
			// 디자이너 BP 가 있으면 사용, 없으면 C++ 클래스로 폴백 (무설정 동작)
			WidgetClass = LoadClass<UUserWidget>(nullptr, TEXT("/Game/AOS/UI/WBP_Minimap.WBP_Minimap_C"));
		}
		if (!WidgetClass)
		{
			WidgetClass = UAOSMinimapWidget::StaticClass();
		}
		if (WidgetClass)
		{
			MinimapWidget = CreateWidget<UAOSMinimapWidget>(this, WidgetClass);
		}
	}

	if (MinimapWidget)
	{
		// UMG 트리(RootWidget) 빌드 후 뷰포트 추가 (AddToViewport 가 슬레이트를 빌드하므로 순서 중요)
		MinimapWidget->ShowMinimap();
		if (!MinimapWidget->IsInViewport())
		{
			MinimapWidget->AddToViewport(5);
		}
	}
}

void AAOSPlayerController::HideMinimap()
{
	if (MinimapWidget)
	{
		MinimapWidget->HideMinimap();
	}
}

// 게임 상태 변경 핸들러
void AAOSPlayerController::OnGameStateChanged(EAOSGameState NewState)
{
	UE_LOG(LogTemp, Warning, TEXT("[PlayerController] 게임 상태 변경: %d"), static_cast<int32>(NewState));

	// Slice 1: 미니맵은 RoundRunning 에서만 — 기본적으로 숨기고 해당 케이스에서 다시 표시
	HideMinimap();
	// 벤픽 위젯도 기본 숨김 (BanPick 케이스에서만 표시)
	HideBanPick();

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

	case EAOSGameState::BanPick:
		HideLobby();
		HideMainMenu();
		HideSettlement();
		HideCharacterSelect();
		ShowBanPick();
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
		// 라운드 시작 → 각 클라이언트 카메라를 자기 팀 CC 로 포커스
		FocusCameraOnOwnCommandCenter();
		// Slice 1: 전투 중 미니맵 표시
		ShowMinimap();
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
	// 메인 메뉴 진입 시 항상 "시작 버튼 안 누름" 상태로 초기화
	bLocalPressedStart = false;

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
			if (MainMenuWidget)
			{
				// 시작 버튼 클릭 델리게이트 바인딩 (중복 방지)
				if (!MainMenuWidget->OnStartClicked.IsAlreadyBound(this, &AAOSPlayerController::OnMainMenuStartClicked))
				{
					MainMenuWidget->OnStartClicked.AddDynamic(this, &AAOSPlayerController::OnMainMenuStartClicked);
				}
			}
		}
	}

	if (MainMenuWidget && !MainMenuWidget->IsInViewport())
	{
		// GameState 팀 준비 상태 구독
		if (AAOSGameState* AOSGS = GetWorld()->GetGameState<AAOSGameState>())
		{
			if (!AOSGS->OnTeamReadyChanged.IsAlreadyBound(this, &AAOSPlayerController::OnMainMenuTeamReadyChanged))
			{
				AOSGS->OnTeamReadyChanged.AddDynamic(this, &AAOSPlayerController::OnMainMenuTeamReadyChanged);
			}
		}
		// 초기 상태: 아무도 시작 안 누른 기본 텍스트
		MainMenuWidget->UpdateReadyState(GetLocalPlayerName(), false, TEXT("다른 사용자"), false);

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
	if (!GetWorld() || GetWorld()->bIsTearingDown)
	{
		return;
	}

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
		AAOSGameState* AOSGS = GetWorld()->GetGameState<AAOSGameState>();
		if (AOSGS && AOSGS->bIsDraw)
		{
			SettlementWidget->SetDraw();
			UE_LOG(LogTemp, Warning, TEXT("[PlayerController] 정산 화면 표시 (무승부)"));
		}
		else
		{
			SettlementWidget->SetResult(WinningTeam);
			UE_LOG(LogTemp, Warning, TEXT("[PlayerController] 정산 화면 표시 (승리: Team%d)"),
				WinningTeam == EAOSTeam::Team1 ? 1 : 2);
		}
		if (!SettlementWidget->IsInViewport())
		{
			SettlementWidget->AddToViewport(10);
		}
		// UI 전용 입력 모드
		SetInputMode(FInputModeUIOnly());
		bShowMouseCursor = true;
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
	// ── 로스터 RPC 전송은 AOSGameMode::TransitionToRoundPreparation()에서 처리 ──
	// GameMode가 모든 PC(원격 클라이언트 서버사이드 PC 포함)를 순회하여
	// Client_ReceiveCharacterRoster를 전송하므로 여기서 별도 전송 불필요.

	// 로컬 컨트롤러만 위젯 생성/표시
	if (!IsLocalPlayerController()) return;

	bAutoSubmittedConfig = false;

	if (!CharacterSelectWidget)
	{
		UClass* WidgetClass = CharacterSelectWidgetClass;
		if (!WidgetClass)
		{
			WidgetClass = LoadClass<UUserWidget>(nullptr, TEXT("/Game/AOS/UI/WBP_CharacterSelect.WBP_CharacterSelect_C"));
		}
		if (!WidgetClass)
		{
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
		// AddToViewport 먼저 → Slate가 Initialize() → BuildUI() → CardGrid 생성
		if (!CharacterSelectWidget->IsInViewport())
		{
			CharacterSelectWidget->AddToViewport(10);
		}
		CharacterSelectWidget->SetVisibility(ESlateVisibility::Visible);

		// 새 라운드 진입 → 로컬 준비 상태 리셋 (버튼 재활성화 + 텍스트 복원)
		CharacterSelectWidget->ResetReadyState();

		// 라운드 번호 표시 (준비 중인 라운드 = 완료된 라운드 + 1)
		if (AAOSGameState* AOSGS = GetWorld()->GetGameState<AAOSGameState>())
		{
			CharacterSelectWidget->SetRoundNumber(AOSGS->GetCurrentRound() + 1);
			// 현재 양 팀 준비 상태도 즉시 반영 (이전 라운드 잔존 상태)
			CharacterSelectWidget->UpdateTeamReadyStatus(AOSGS->bTeam1Ready, AOSGS->bTeam2Ready);
		}

		// 이미 캐시된 로스터가 있으면 (서버이거나 RPC가 먼저 도착한 경우) 즉시 표시
		CharacterSelectWidget->InitializeWithRoster(CachedCharacterRoster);
		UE_LOG(LogTemp, Warning, TEXT("[PlayerController] 캐릭터 선택 UI 표시 (로스터: %d개)"),
			CachedCharacterRoster.Num());
	}
}

void AAOSPlayerController::Client_ReceiveCharacterRoster_Implementation(
	const TArray<FCharacterRosterEntry>& Roster)
{
	CachedCharacterRoster = Roster;
	UE_LOG(LogTemp, Warning, TEXT("[PlayerController] 클라이언트 로스터 수신: %d개"), Roster.Num());

	// 위젯이 존재하면 즉시 갱신 (가시성 여부 무관 — 타이밍 이슈 방지)
	if (IsValid(CharacterSelectWidget))
	{
		CharacterSelectWidget->InitializeWithRoster(CachedCharacterRoster);
	}
	if (IsValid(BanPickWidget))
	{
		BanPickWidget->InitializeWithRoster(CachedCharacterRoster);
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

// 벤픽 UI 표시 (로컬 컨트롤러 전용 — ShowCharacterSelect 패턴)
void AAOSPlayerController::ShowBanPick()
{
	if (!IsLocalPlayerController()) return;

	if (!BanPickWidget)
	{
		UClass* WidgetClass = BanPickWidgetClass;
		if (!WidgetClass)
		{
			WidgetClass = LoadClass<UUserWidget>(nullptr, TEXT("/Game/AOS/UI/WBP_BanPick.WBP_BanPick_C"));
		}
		if (!WidgetClass)
		{
			WidgetClass = UAOSBanPickWidget::StaticClass();
		}
		if (WidgetClass)
		{
			BanPickWidget = CreateWidget<UAOSBanPickWidget>(this, WidgetClass);
		}
	}

	if (BanPickWidget)
	{
		// ⚠ WidgetTree->RootWidget 을 먼저 채운 뒤 AddToViewport — 빈 위젯 실현 방지(미니맵 교훈)
		BanPickWidget->InitializeWithRoster(CachedCharacterRoster);
		// 3D 프리뷰 스테이지 스폰 후 위젯에 연결 (클라 전용)
		EnsureDraftPreviewStages();
		BanPickWidget->SetPreviewStages(MyPreviewStage, EnemyPreviewStage);
		if (!BanPickWidget->IsInViewport())
		{
			BanPickWidget->AddToViewport(20);
		}
		BanPickWidget->SetVisibility(ESlateVisibility::Visible);
		UE_LOG(LogTemp, Warning, TEXT("[PlayerController] 벤픽 UI 표시 (로스터: %d개)"), CachedCharacterRoster.Num());
	}
}

void AAOSPlayerController::HideBanPick()
{
	if (BanPickWidget && BanPickWidget->GetVisibility() != ESlateVisibility::Collapsed)
	{
		BanPickWidget->SetVisibility(ESlateVisibility::Collapsed);
		UE_LOG(LogTemp, Warning, TEXT("[PlayerController] 벤픽 UI 숨김"));
	}
	// BanPick 이탈 시 프리뷰 스테이지 정리 (BanPick 동안만 존재)
	DestroyDraftPreviewStages();
}

// 벤픽 3D 프리뷰 스테이지 스폰 (클라 전용 — 서버는 렌더 없음)
void AAOSPlayerController::EnsureDraftPreviewStages()
{
	if (!IsLocalPlayerController() || GetNetMode() == NM_DedicatedServer) return;
	UWorld* World = GetWorld();
	if (!World) return;

	FActorSpawnParameters Params;
	Params.Owner = this;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	// 렌더 타깃 해상도 (세로 캐릭터 프레이밍)
	const int32 RTW = 360, RTH = 640;

	if (!MyPreviewStage)
	{
		MyPreviewStage = World->SpawnActor<AAOSCharacterPreviewStage>(
			AAOSCharacterPreviewStage::StaticClass(),
			FVector(0.f, 0.f, 100000.f), FRotator::ZeroRotator, Params);
		if (MyPreviewStage)
		{
			MyPreviewStage->InitRenderTarget(RTW, RTH);
		}
	}
	if (!EnemyPreviewStage)
	{
		// 두 스테이지를 떨어뜨려 스폰 (ShowOnlyList 로 격리되지만 안전 마진)
		EnemyPreviewStage = World->SpawnActor<AAOSCharacterPreviewStage>(
			AAOSCharacterPreviewStage::StaticClass(),
			FVector(4000.f, 0.f, 100000.f), FRotator::ZeroRotator, Params);
		if (EnemyPreviewStage)
		{
			EnemyPreviewStage->InitRenderTarget(RTW, RTH);
		}
	}
}

void AAOSPlayerController::DestroyDraftPreviewStages()
{
	if (MyPreviewStage)
	{
		MyPreviewStage->Destroy();
		MyPreviewStage = nullptr;
	}
	if (EnemyPreviewStage)
	{
		EnemyPreviewStage->Destroy();
		EnemyPreviewStage = nullptr;
	}
}

// 벤픽: 클라 → 서버 밴/픽 선택 (서버가 PlayerState 팀으로 강제)
void AAOSPlayerController::Server_DraftSelect_Implementation(int32 UnitId)
{
	if (!HasAuthority()) return;
	AAOSPlayerState* PS = GetPlayerState<AAOSPlayerState>();
	if (!PS) return;
	if (AAOSGameMode* GM = GetWorld()->GetAuthGameMode<AAOSGameMode>())
	{
		GM->ServerApplyDraftSelection(PS->GetTeam(), UnitId);
	}
}

// 벤픽 치트 RPC — 서버에서 드래프트 일괄 자동 완성 (어느 클라가 눌러도 양 팀 전부 채움)
void AAOSPlayerController::Server_AutoCompleteDraft_Implementation()
{
	if (!HasAuthority()) return;
	if (AAOSGameMode* GM = GetWorld()->GetAuthGameMode<AAOSGameMode>())
	{
		GM->ServerAutoCompleteDraft();
	}
}

// 테스트용 콘솔 명령 — 로컬 입력 → Server RPC 로 전달
void AAOSPlayerController::AutoDraft()
{
	Server_AutoCompleteDraft();
	UE_LOG(LogTemp, Warning, TEXT("[PlayerController] AutoDraft 콘솔 명령 → 서버에 드래프트 자동 완성 요청"));
}

// 캐릭터 선택 UI에서 라운드 시작 클릭
void AAOSPlayerController::OnStartRoundClicked()
{
	if (CharacterSelectWidget)
	{
		const EAOSLane Lanes[] = { EAOSLane::Top, EAOSLane::Mid, EAOSLane::Bottom };
		for (EAOSLane Lane : Lanes)
		{
			TArray<TSubclassOf<AAOSCharacter>> Classes = CharacterSelectWidget->GetLaneClasses(Lane);
			TArray<int32> UnitIds = CharacterSelectWidget->GetLaneUnitIds(Lane);
			Server_SetLaneDeployClasses(Lane, Classes, UnitIds);
			Server_SetLaneDeployCount(Lane, Classes.Num()); // 하위 호환
		}

		UE_LOG(LogTemp, Warning, TEXT("[PlayerController] 라운드 시작 요청 (Top:%d, Mid:%d, Bottom:%d)"),
			CharacterSelectWidget->GetLaneCount(EAOSLane::Top),
			CharacterSelectWidget->GetLaneCount(EAOSLane::Mid),
			CharacterSelectWidget->GetLaneCount(EAOSLane::Bottom));
	}

	Server_SetReady(true);
}

// Server RPC 구현 - 라인별 배치 수 설정 (하위 호환)
bool AAOSPlayerController::Server_SetLaneDeployCount_Validate(EAOSLane Lane, int32 Count)
{
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

// Server RPC 구현 - 라인별 배치 클래스 목록 설정
bool AAOSPlayerController::Server_SetLaneDeployClasses_Validate(EAOSLane Lane,
	const TArray<TSubclassOf<AAOSCharacter>>& Classes,
	const TArray<int32>& UnitIds)
{
	return Classes.Num() <= 2;
}

void AAOSPlayerController::Server_SetLaneDeployClasses_Implementation(EAOSLane Lane,
	const TArray<TSubclassOf<AAOSCharacter>>& Classes,
	const TArray<int32>& UnitIds)
{
	AAOSGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<AAOSGameMode>() : nullptr;
	AAOSPlayerState* PS = GetPlayerState<AAOSPlayerState>();

	if (GM && PS)
	{
		GM->ServerSetLaneDeployClassesForPlayer(PS, Lane, Classes, UnitIds);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[PlayerController] Server_SetLaneDeployClasses: GameMode 또는 PlayerState 없음"));
	}
}

// Phase 3A: Server RPC 구현 - 준비 상태 토글
void AAOSPlayerController::Server_SetReady_Implementation(bool bReady)
{
	AAOSGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<AAOSGameMode>() : nullptr;
	AAOSPlayerState* PS = GetPlayerState<AAOSPlayerState>();

	if (GM && PS)
	{
		UE_LOG(LogTemp, Warning, TEXT("[PlayerController] Server_SetReady: %s → bReady=%d"),
			PS->GetTeam() == EAOSTeam::Team1 ? TEXT("Team1") : TEXT("Team2"), bReady ? 1 : 0);
		GM->ServerSetPlayerReady(PS, bReady);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[PlayerController] Server_SetReady: GameMode(%s) 또는 PlayerState(%s) 없음"),
			GM ? TEXT("OK") : TEXT("NULL"),
			PS ? TEXT("OK") : TEXT("NULL"));
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

// Server RPC 구현 - 유닛 아이템 구매 (서버가 자기 팀 골드로 검증)
void AAOSPlayerController::Server_BuyItemForUnit_Implementation(int32 UnitId, FName ItemRowName)
{
	AAOSGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<AAOSGameMode>() : nullptr;
	AAOSPlayerState* PS = GetPlayerState<AAOSPlayerState>();
	if (!GM || !PS)
	{
		return;
	}

	// 클라가 임의 팀을 지정 못 하도록 서버가 PlayerState 의 팀으로 강제
	GM->ServerBuyItemForUnit(PS->GetTeam(), UnitId, ItemRowName);
}

// 테스트용 콘솔 명령 — 로컬에서 입력 → Server RPC 로 전달 (UnitId = 로스터 인덱스)
void AAOSPlayerController::BuyItem(int32 UnitId, FName ItemRowName)
{
	Server_BuyItemForUnit(UnitId, ItemRowName);
	UE_LOG(LogTemp, Log, TEXT("[PlayerController] BuyItem 콘솔 명령 → 서버 요청 (UnitId=%d, Item=%s)"),
		UnitId, *ItemRowName.ToString());
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
		// 준비 버튼 바인딩 (중복 방지)
		if (!LobbyWidget->OnReadyClicked.IsAlreadyBound(this, &AAOSPlayerController::OnLobbyReadyClicked))
		{
			LobbyWidget->OnReadyClicked.AddDynamic(this, &AAOSPlayerController::OnLobbyReadyClicked);
		}

		// GameState 팀 준비 상태 변경 + 접속 인원 변경 구독 (중복 방지)
		// ─ 먼저 구독한 뒤 초기값을 읽어야 OnRep_* 와 경합하지 않는다 ─
		if (AAOSGameState* AOSGS = GetWorld()->GetGameState<AAOSGameState>())
		{
			if (!AOSGS->OnTeamReadyChanged.IsAlreadyBound(this, &AAOSPlayerController::OnTeamReadyChanged))
			{
				AOSGS->OnTeamReadyChanged.AddDynamic(this, &AAOSPlayerController::OnTeamReadyChanged);
			}
			if (!AOSGS->OnPlayerCountChanged.IsAlreadyBound(this, &AAOSPlayerController::OnLobbyPlayerCountChanged))
			{
				AOSGS->OnPlayerCountChanged.AddDynamic(this, &AAOSPlayerController::OnLobbyPlayerCountChanged);
			}

			// 현재 상태 즉시 반영
			// PlayerArray.Num() 과 ConnectedCount 중 더 큰 값 사용 (리플리케이션 타이밍 차이 방어)
			const int32 PlayerArrayCount = GetWorld()->GetGameState()
				? GetWorld()->GetGameState()->PlayerArray.Num() : 0;
			const int32 DisplayCount = FMath::Max(PlayerArrayCount, AOSGS->ConnectedCount);

			LobbyWidget->UpdateReadyState(
				GetPlayerNameByTeam(EAOSTeam::Team1), AOSGS->bTeam1Ready,
				GetPlayerNameByTeam(EAOSTeam::Team2), AOSGS->bTeam2Ready);
			LobbyWidget->UpdatePlayerCount(DisplayCount, 2);

			UE_LOG(LogTemp, Warning, TEXT("[PlayerController] 로비 화면 표시 — PlayerArray=%d, ConnectedCount=%d, Display=%d/2"),
				PlayerArrayCount, AOSGS->ConnectedCount, DisplayCount);
		}
		else
		{
			LobbyWidget->UpdatePlayerCount(0, 2);
			UE_LOG(LogTemp, Warning, TEXT("[PlayerController] 로비 화면 표시 — GameState 없음"));
		}

		if (!LobbyWidget->IsInViewport())
		{
			LobbyWidget->AddToViewport(10);
		}
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

// 준비 버튼 클릭 → 서버에 준비 상태 전달
void AAOSPlayerController::OnLobbyReadyClicked()
{
	UE_LOG(LogTemp, Warning, TEXT("[PlayerController] 준비 버튼 클릭 → Server_SetReady(true)"));
	Server_SetReady(true);
}

// GameState 접속 인원 변경 → 로비 위젯 갱신 (이름도 함께 갱신: 신규 플레이어 접속 시 반영)
void AAOSPlayerController::OnLobbyPlayerCountChanged(int32 Count)
{
	if (!LobbyWidget)
	{
		return;
	}

	LobbyWidget->UpdatePlayerCount(Count, 2);

	// 인원 변경 시 이름도 갱신 (새 플레이어가 접속하면 PlayerArray에 추가된 이름을 반영)
	if (AAOSGameState* AOSGS = GetWorld()->GetGameState<AAOSGameState>())
	{
		LobbyWidget->UpdateReadyState(
			GetPlayerNameByTeam(EAOSTeam::Team1), AOSGS->bTeam1Ready,
			GetPlayerNameByTeam(EAOSTeam::Team2), AOSGS->bTeam2Ready);
	}

	UE_LOG(LogTemp, Warning, TEXT("[PlayerController] 접속 인원 갱신: %d/2"), Count);
}

// GameState 팀 준비 상태 변경 → 로비 위젯 갱신
void AAOSPlayerController::OnTeamReadyChanged()
{
	AAOSGameState* AOSGS = GetWorld()->GetGameState<AAOSGameState>();
	if (!AOSGS)
	{
		return;
	}

	// 로비 위젯 갱신
	if (LobbyWidget)
	{
		LobbyWidget->UpdateReadyState(
			GetPlayerNameByTeam(EAOSTeam::Team1), AOSGS->bTeam1Ready,
			GetPlayerNameByTeam(EAOSTeam::Team2), AOSGS->bTeam2Ready);
	}

	// 캐릭터 선택(라운드 준비) 위젯 갱신 — 보여지는 동안에만 의미 있음
	if (CharacterSelectWidget)
	{
		CharacterSelectWidget->UpdateTeamReadyStatus(AOSGS->bTeam1Ready, AOSGS->bTeam2Ready);
	}

	UE_LOG(LogTemp, Warning, TEXT("[PlayerController] 팀 준비 상태 갱신 — Team1:%d Team2:%d"),
		AOSGS->bTeam1Ready, AOSGS->bTeam2Ready);
}

// 메인 메뉴 시작 버튼 클릭 → 즉시 로컬 UI 갱신 + 서버에 Ready 전달
void AAOSPlayerController::OnMainMenuStartClicked()
{
	// 로컬 플래그 세팅: 이제부터 OnMainMenuTeamReadyChanged가 UI를 갱신할 수 있음
	bLocalPressedStart = true;

	// 서버 응답 전에 즉시 로컬 UI 갱신
	// (자신: 준비 완료, 상대방: 대기 중 — 상대방 이름은 아직 알 수 없으므로 "다른 사용자")
	if (MainMenuWidget)
	{
		MainMenuWidget->UpdateReadyState(GetLocalPlayerName(), true, TEXT("다른 사용자"), false);
	}

	UE_LOG(LogTemp, Warning, TEXT("[PlayerController] 메인 메뉴 시작 클릭 → Server_SetReady(true)"));
	Server_SetReady(true);
}

// GameState 팀 준비 상태 변경 → 메인 메뉴 StatusText 갱신
void AAOSPlayerController::OnMainMenuTeamReadyChanged()
{
	// ── 요구사항 1 ─────────────────────────────────────────────────────────────
	// 자신이 "게임 시작"을 누르기 전까지는 상대방 준비 상태를 표시하지 않는다.
	// → 서버가 먼저 눌렀을 때 클라이언트 화면에 "Team1: 준비 완료" 같은 정보가 뜨지 않음.
	// ─────────────────────────────────────────────────────────────────────────
	if (!bLocalPressedStart)
	{
		return;
	}

	if (!MainMenuWidget || !MainMenuWidget->IsInViewport())
	{
		return;
	}

	if (AAOSGameState* AOSGS = GetWorld()->GetGameState<AAOSGameState>())
	{
		// 로컬 플레이어의 팀을 기준으로 LocalReady / RemoteReady 결정
		AAOSPlayerState* LocalPS = GetPlayerState<AAOSPlayerState>();
		bool bLocalReady  = false;
		bool bRemoteReady = false;
		EAOSTeam RemoteTeam = EAOSTeam::Team2;

		if (LocalPS)
		{
			if (LocalPS->GetTeam() == EAOSTeam::Team1)
			{
				bLocalReady  = AOSGS->bTeam1Ready;
				bRemoteReady = AOSGS->bTeam2Ready;
				RemoteTeam   = EAOSTeam::Team2;
			}
			else
			{
				bLocalReady  = AOSGS->bTeam2Ready;
				bRemoteReady = AOSGS->bTeam1Ready;
				RemoteTeam   = EAOSTeam::Team1;
			}
		}

		FString LocalName  = GetLocalPlayerName();
		FString RemoteName = GetPlayerNameByTeam(RemoteTeam);
		if (RemoteName.IsEmpty() || RemoteName == TEXT("Team1") || RemoteName == TEXT("Team2"))
		{
			RemoteName = TEXT("다른 사용자");
		}

		MainMenuWidget->UpdateReadyState(LocalName, bLocalReady, RemoteName, bRemoteReady);
		UE_LOG(LogTemp, Warning, TEXT("[PlayerController] 메인 메뉴 팀 준비 갱신 — Local(%s):%d Remote(%s):%d"),
			*LocalName, bLocalReady ? 1 : 0, *RemoteName, bRemoteReady ? 1 : 0);
	}
}

// 로컬 플레이어 이름 반환 (PlayerState에서 읽음, 없으면 "플레이어" 반환)
FString AAOSPlayerController::GetLocalPlayerName() const
{
	if (APlayerState* PS = GetPlayerState<APlayerState>())
	{
		FString Name = PS->GetPlayerName();
		if (!Name.IsEmpty())
		{
			return Name;
		}
	}
	return TEXT("플레이어");
}

// 특정 팀 플레이어 이름 반환 (GameState PlayerArray에서 탐색)
FString AAOSPlayerController::GetPlayerNameByTeam(EAOSTeam Team) const
{
	AGameStateBase* GS = GetWorld() ? GetWorld()->GetGameState() : nullptr;
	if (!GS)
	{
		return Team == EAOSTeam::Team1 ? TEXT("Team1") : TEXT("Team2");
	}

	for (APlayerState* PS : GS->PlayerArray)
	{
		AAOSPlayerState* AOSPS = Cast<AAOSPlayerState>(PS);
		if (AOSPS && AOSPS->GetTeam() == Team)
		{
			FString Name = PS->GetPlayerName();
			if (!Name.IsEmpty())
			{
				return Name;
			}
			return Team == EAOSTeam::Team1 ? TEXT("Team1") : TEXT("Team2");
		}
	}
	return Team == EAOSTeam::Team1 ? TEXT("Team1") : TEXT("Team2");
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

// F1: 타워/커맨드센터 구조물 박스 표시 토글
void AAOSPlayerController::DebugToggleStructureBoxes()
{
	if (!IsLocalPlayerController()) return;

	IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("AOS.Debug.ShowStructureBoxes"));
	if (CVar)
	{
		int32 NewVal = CVar->GetInt() ? 0 : 1;
		CVar->Set(NewVal);
		UE_LOG(LogTemp, Warning, TEXT("[Debug] AOS.Debug.ShowStructureBoxes = %d"), NewVal);
		GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::White,
			FString::Printf(TEXT("[Debug] 구조물 박스: %s"), NewVal ? TEXT("ON") : TEXT("OFF")));
	}
}

// F2: 공격 범위 표시 토글
void AAOSPlayerController::DebugToggleAttackRange()
{
	if (!IsLocalPlayerController()) return;

	IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("AOS.Debug.ShowAttackRange"));
	if (CVar)
	{
		int32 NewVal = CVar->GetInt() ? 0 : 1;
		CVar->Set(NewVal);
		UE_LOG(LogTemp, Warning, TEXT("[Debug] AOS.Debug.ShowAttackRange = %d"), NewVal);
		GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::White,
			FString::Printf(TEXT("[Debug] 공격 범위: %s"), NewVal ? TEXT("ON") : TEXT("OFF")));
	}
}

// F3: AI 캐릭터 이동 경로 표시 토글
void AAOSPlayerController::DebugToggleCharacterPaths()
{
	if (!IsLocalPlayerController()) return;

	IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("AOS.Debug.ShowCharacterPaths"));
	if (CVar)
	{
		int32 NewVal = CVar->GetInt() ? 0 : 1;
		CVar->Set(NewVal);
		UE_LOG(LogTemp, Warning, TEXT("[Debug] AOS.Debug.ShowCharacterPaths = %d"), NewVal);
		GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::White,
			FString::Printf(TEXT("[Debug] 캐릭터 경로: %s"), NewVal ? TEXT("ON") : TEXT("OFF")));
	}
}
