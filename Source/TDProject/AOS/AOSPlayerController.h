#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "AOSGameMode.h"
#include "AOSPlayerController.generated.h"

class AAOSCharacter;
class ACameraActor;
class UAOSMainMenuWidget;
class UAOSSettlementWidget;

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
	void StartGameFromPreparation();

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

protected:
	// UI 위젯 클래스 (에디터에서 설정)
	UPROPERTY(EditDefaultsOnly, Category = "AOS|UI")
	TSubclassOf<UUserWidget> MainMenuWidgetClass;

	UPROPERTY(EditDefaultsOnly, Category = "AOS|UI")
	TSubclassOf<UUserWidget> SettlementWidgetClass;

	// UI 위젯 인스턴스 (런타임)
	UPROPERTY()
	UAOSMainMenuWidget* MainMenuWidget;

	UPROPERTY()
	UAOSSettlementWidget* SettlementWidget;

	// 게임 상태 변경 핸들러
	UFUNCTION()
	void OnGameStateChanged(EAOSGameState NewState);

	// UI 표시/숨김 함수
	void ShowMainMenu();
	void HideMainMenu();
	void ShowSettlement(EAOSTeam WinningTeam);
	void HideSettlement();

	UPROPERTY(BlueprintReadOnly, Category = "AOS|Game")
	EAOSTeam PlayerTeam = EAOSTeam::Team1;

	UPROPERTY(BlueprintReadOnly, Category = "AOS|Characters")
	TArray<AAOSCharacter*> PlayerCharacters;

	UPROPERTY(BlueprintReadOnly, Category = "AOS|Deployment")
	TArray<EAOSLane> CurrentDeployment;

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

private:
	// 🟡 MODIFIED - 카메라 이동 방향을 X, Y 분리
	float CameraMoveForward = 0.0f;
	float CameraMoveRight = 0.0f;
};
