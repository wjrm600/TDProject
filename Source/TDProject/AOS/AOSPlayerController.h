#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "AOSGameMode.h"
#include "AOSPlayerController.generated.h"

class AAOSCharacter;
class ACameraActor;
class UAOSMainMenuWidget;
class UAOSSettlementWidget;
class UAOSCharacterSelectWidget;
class UAOSLobbyWidget;
class UAOSMinimapWidget;
class UAOSBanPickWidget;

/**
 * AOS 게임의 플레이어 컨트롤러
 * RTS 스타일 카메라 제어, 캐릭터 선택, 게임 상태 관리를 담당
 */
UCLASS()
class TDPROJECT_API AAOSPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AAOSPlayerController();

	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;
	virtual void Tick(float DeltaTime) override;

	// 🟢 NEW - 선택된 캐릭터 관련
	UFUNCTION(BlueprintCallable, Category = "AOS|Selection")
	void SelectCharacter(AAOSCharacter* NewCharacter);

	UFUNCTION(BlueprintCallable, Category = "AOS|Selection")
	AAOSCharacter* GetSelectedCharacter() const { return SelectedCharacter; }

	// 🟢 NEW - 카메라 제어
	UFUNCTION(BlueprintCallable, Category = "AOS|Camera")
	void MoveCameraForward(float AxisValue);

	UFUNCTION(BlueprintCallable, Category = "AOS|Camera")
	void MoveCameraRight(float AxisValue);

	UFUNCTION(BlueprintCallable, Category = "AOS|Camera")
	void SetCameraHeight(float Height);

	UFUNCTION(BlueprintCallable, Category = "AOS|Camera")
	void SetCameraAngle(float Pitch, float Yaw);

	UFUNCTION(BlueprintCallable, Category = "AOS|Camera")
	void ZoomCamera(float AxisValue);

	// 라운드 시작 시: 로컬 클라이언트 카메라를 자기 팀 커맨드 센터로 포커스
	// (현재 회전·줌은 유지하고 XY 위치만 이동하여 CC 가 화면 중앙에 오도록)
	UFUNCTION(BlueprintCallable, Category = "AOS|Camera")
	void FocusCameraOnOwnCommandCenter();

	// 지면(z=0) 한 점이 화면 중앙에 오도록 RTS 카메라 XY 이동 (회전·줌 유지). 로컬 전용.
	// 미니맵 클릭 이동 + FocusCameraOnOwnCommandCenter 가 공유.
	UFUNCTION(BlueprintCallable, Category = "AOS|Camera")
	void MoveCameraToGroundPoint(const FVector& GroundLocation);

	// 미니맵 방향 정렬용: 현재 카메라 yaw (RTSCamera 우선)
	UFUNCTION(BlueprintCallable, Category = "AOS|Camera")
	float GetCameraYaw() const;

	// 캐릭터 배치 관련
	UFUNCTION(BlueprintCallable, Category = "AOS|Deployment")
	void SetCharacterDeployment(const TArray<EAOSLane>& LaneAssignments);

	UFUNCTION(BlueprintCallable, Category = "AOS|Deployment")
	void DeployCharactersToLanes();

	UFUNCTION(BlueprintCallable, Category = "AOS|Game")
	void StartRound();

	// 플레이어의 캐릭터 목록
	UFUNCTION(BlueprintCallable, Category = "AOS|Characters")
	TArray<AAOSCharacter*> GetPlayerCharacters() const { return PlayerCharacters; }

	// 팀 정보
	UFUNCTION(BlueprintCallable, Category = "AOS|Game")
	void SetPlayerTeam(EAOSTeam Team);

	UFUNCTION(BlueprintCallable, Category = "AOS|Game")
	EAOSTeam GetPlayerTeam() const { return PlayerTeam; }

	// 캐릭터 선택 확인
	UFUNCTION(BlueprintCallable, Category = "AOS|Selection")
	void HandleMouseClick();

	// Phase 3A: 서버 RPC (클라이언트 → 서버 요청)
	// 라인별 배치 수 설정 (서버 검증: 0~2)
	UFUNCTION(Server, Reliable, WithValidation, Category = "AOS|Network")
	void Server_SetLaneDeployCount(EAOSLane Lane, int32 Count);

	// 라인별 배치 클래스 목록 설정 (드래그앤드롭 UI 용)
	// UnitIds: 각 클래스의 UnitId(로스터 인덱스) — Classes 와 같은 길이. 유닛 귀속 아이템 적용에 사용.
	UFUNCTION(Server, Reliable, WithValidation, Category = "AOS|Network")
	void Server_SetLaneDeployClasses(EAOSLane Lane,
		const TArray<TSubclassOf<AAOSCharacter>>& Classes,
		const TArray<int32>& UnitIds);

	// 준비 상태 토글 (로비/라운드 준비 시 사용)
	UFUNCTION(Server, Reliable, Category = "AOS|Network")
	void Server_SetReady(bool bReady);

	// 라운드 시작 요청 (서버에서 조건 검증 후 StartRound 호출)
	UFUNCTION(Server, Reliable, Category = "AOS|Network")
	void Server_RequestStartRound();

	// 유닛 아이템 구매 요청 (클라 → 서버). 상점 UI 가 호출. 서버가 골드/단계 검증.
	UFUNCTION(Server, Reliable, Category = "AOS|Network")
	void Server_BuyItemForUnit(int32 UnitId, FName ItemRowName);

	// 테스트용 콘솔 명령 — 예) "BuyItem 0 Sword" (UnitId = 로스터 인덱스)
	UFUNCTION(Exec)
	void BuyItem(int32 UnitId, FName ItemRowName);

	// 서버 → 클라이언트: 캐릭터 로스터 전달
	UFUNCTION(Client, Reliable, Category = "AOS|Network")
	void Client_ReceiveCharacterRoster(const TArray<FCharacterRosterEntry>& Roster);

	// 벤픽: 클라 → 서버 밴/픽 선택 (서버가 PlayerState 팀으로 강제)
	UFUNCTION(Server, Reliable, Category = "AOS|Network")
	void Server_DraftSelect(int32 UnitId);

protected:
	// UI 위젯 클래스 (에디터에서 설정)
	UPROPERTY(EditDefaultsOnly, Category = "AOS|UI")
	TSubclassOf<UUserWidget> MainMenuWidgetClass;

	UPROPERTY(EditDefaultsOnly, Category = "AOS|UI")
	TSubclassOf<UUserWidget> SettlementWidgetClass;

	UPROPERTY(EditDefaultsOnly, Category = "AOS|UI")
	TSubclassOf<UUserWidget> CharacterSelectWidgetClass;

	// 벤픽 위젯 클래스 (미설정 시 C++ UAOSBanPickWidget 폴백)
	UPROPERTY(EditDefaultsOnly, Category = "AOS|UI")
	TSubclassOf<UUserWidget> BanPickWidgetClass;

	UPROPERTY(EditDefaultsOnly, Category = "AOS|UI")
	TSubclassOf<UUserWidget> LobbyWidgetClass;

	// 미니맵 위젯 클래스 (미설정 시 C++ UAOSMinimapWidget 폴백 — 무설정 동작)
	UPROPERTY(EditDefaultsOnly, Category = "AOS|UI")
	TSubclassOf<UUserWidget> MinimapWidgetClass;

	// UI 위젯 인스턴스 (런타임)
	UPROPERTY()
	UAOSMainMenuWidget* MainMenuWidget;

	UPROPERTY()
	UAOSSettlementWidget* SettlementWidget;

	UPROPERTY()
	UAOSCharacterSelectWidget* CharacterSelectWidget = nullptr;

	UPROPERTY()
	UAOSBanPickWidget* BanPickWidget = nullptr;

	UPROPERTY()
	UAOSLobbyWidget* LobbyWidget = nullptr;

	UPROPERTY()
	UAOSMinimapWidget* MinimapWidget = nullptr;

	// 게임 상태 변경 핸들러
	UFUNCTION()
	void OnGameStateChanged(EAOSGameState NewState);

	// UI 표시/숨김 함수
	void ShowMainMenu();
	void HideMainMenu();
	void ShowSettlement(EAOSTeam WinningTeam);
	void HideSettlement();
	void ShowCharacterSelect();
	void HideCharacterSelect();
	void ShowBanPick();
	void HideBanPick();
	void ShowLobby();
	void HideLobby();

	// Slice 1: 미니맵 표시/숨김 (라운드 진행 중에만 표시). 로컬 컨트롤러 전용.
	void ShowMinimap();
	void HideMinimap();

	UFUNCTION()
	void OnStartRoundClicked();

	// 로비 준비 버튼 클릭 → 서버 RPC 호출
	UFUNCTION()
	void OnLobbyReadyClicked();

	// GameState 접속 인원 변경 → 로비 UI 갱신
	UFUNCTION()
	void OnLobbyPlayerCountChanged(int32 Count);

	// GameState 팀 준비 상태 변경 → 로비 UI 갱신
	UFUNCTION()
	void OnTeamReadyChanged();

	// 메인 메뉴 "게임 시작" 버튼 클릭 → Server_SetReady(true) 호출
	UFUNCTION()
	void OnMainMenuStartClicked();

	// GameState 팀 준비 상태 변경 → 메인 메뉴 StatusText 갱신
	UFUNCTION()
	void OnMainMenuTeamReadyChanged();

	// 플레이어 이름 헬퍼 (UI 표시용)
	FString GetLocalPlayerName() const;
	FString GetPlayerNameByTeam(EAOSTeam Team) const;

	UPROPERTY(BlueprintReadOnly, Category = "AOS|Game")
	EAOSTeam PlayerTeam = EAOSTeam::Team1;

	UPROPERTY(BlueprintReadOnly, Category = "AOS|Characters")
	TArray<AAOSCharacter*> PlayerCharacters;

	UPROPERTY(BlueprintReadOnly, Category = "AOS|Deployment")
	TArray<EAOSLane> CurrentDeployment;

	// 서버에서 받은 캐릭터 로스터 캐시 (클라이언트에서도 사용)
	TArray<FCharacterRosterEntry> CachedCharacterRoster;

	// 라운드별 라인 배치 수 (하위 호환)
	TMap<EAOSLane, int32> LocalDeployPlan;

	// 라운드별 라인 배치 클래스 (드래그앤드롭 UI)
	TMap<EAOSLane, TArray<TSubclassOf<AAOSCharacter>>> LocalDeployPlanClasses;

	UPROPERTY(BlueprintReadOnly, Category = "AOS|Game")
	class AAOSGameMode* GameMode;

	// 🟢 NEW - RTS 카메라 및 선택 관련
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AOS|Camera")
	class ACameraActor* RTSCamera;

	UPROPERTY(BlueprintReadOnly, Category = "AOS|Selection")
	AAOSCharacter* SelectedCharacter;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AOS|Camera")
	float CameraHeight = 12000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AOS|Camera")
	float CameraPitch = -70.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AOS|Camera")
	float CameraYaw = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AOS|Camera")
	float CameraMoveSpeed = 8000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AOS|Camera")
	float ZoomSpeed = 2000.0f;

	// 줌 인 한계 — 카메라가 가장 가까이(확대)된 상태의 높이.
	// ⚠ 높이 기반이라 값이 "작을수록" 더 확대(줌 인)됩니다. 더 당겨 보려면 이 값을 낮추세요.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AOS|Camera")
	float MaxZoomInHeight = 800.0f;

	// 줌 아웃 한계 — 카메라가 가장 멀리(축소)된 상태의 높이.
	// 값이 "클수록" 더 축소(줌 아웃)됩니다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AOS|Camera")
	float MaxZoomOutHeight = 20000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AOS|Camera")
	float MapBoundaryX = 40000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AOS|Camera")
	float MapBoundaryY = 40000.0f;

	// 메인메뉴 레벨용 카메라 설정
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AOS|Camera")
	FVector MenuCameraLocation = FVector(300.0f, 0.0f, 100.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AOS|Camera")
	FRotator MenuCameraRotation = FRotator(-10.0f, 180.0f, 0.0f);

private:
	// 🟡 MODIFIED - 카메라 이동 방향을 X, Y 분리
	float CameraMoveForward = 0.0f;
	float CameraMoveRight = 0.0f;

	// 메인 메뉴에서 로컬 플레이어가 "게임 시작"을 눌렀는지 여부
	// false → 상대방 Ready 상태를 UI에 표시하지 않음 (요구사항 1)
	bool bLocalPressedStart = false;

	// 준비 단계 자동 배치 전송 완료 여부 (타이머 만료 전 자동 제출 중복 방지)
	bool bAutoSubmittedConfig = false;

	// 디버그 치트키
	void DebugToggleStructureBoxes();
	void DebugToggleAttackRange();
	void DebugToggleCharacterPaths();
};
