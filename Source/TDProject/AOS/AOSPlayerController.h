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

	// 준비 상태 토글 (로비/라운드 준비 시 사용)
	UFUNCTION(Server, Reliable, Category = "AOS|Network")
	void Server_SetReady(bool bReady);

	// 라운드 시작 요청 (서버에서 조건 검증 후 StartRound 호출)
	UFUNCTION(Server, Reliable, Category = "AOS|Network")
	void Server_RequestStartRound();

protected:
	// UI 위젯 클래스 (에디터에서 설정)
	UPROPERTY(EditDefaultsOnly, Category = "AOS|UI")
	TSubclassOf<UUserWidget> MainMenuWidgetClass;

	UPROPERTY(EditDefaultsOnly, Category = "AOS|UI")
	TSubclassOf<UUserWidget> SettlementWidgetClass;

	UPROPERTY(EditDefaultsOnly, Category = "AOS|UI")
	TSubclassOf<UUserWidget> CharacterSelectWidgetClass;

	UPROPERTY(EditDefaultsOnly, Category = "AOS|UI")
	TSubclassOf<UUserWidget> LobbyWidgetClass;

	// UI 위젯 인스턴스 (런타임)
	UPROPERTY()
	UAOSMainMenuWidget* MainMenuWidget;

	UPROPERTY()
	UAOSSettlementWidget* SettlementWidget;

	UPROPERTY()
	UAOSCharacterSelectWidget* CharacterSelectWidget = nullptr;

	UPROPERTY()
	UAOSLobbyWidget* LobbyWidget = nullptr;

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
	void ShowLobby();
	void HideLobby();

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

	// 라운드별 라인 배치 수 (캐릭터 선택 UI에서 설정)
	TMap<EAOSLane, int32> LocalDeployPlan;

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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AOS|Camera")
	float MinZoomHeight = 4000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AOS|Camera")
	float MaxZoomHeight = 20000.0f;

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

	// 디버그 치트키
	void DebugToggleStructureBoxes();
	void DebugToggleAttackRange();
	void DebugToggleCharacterPaths();
};
