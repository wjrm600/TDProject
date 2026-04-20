#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "AOSMainMenuWidget.generated.h"

class UButton;
class UTextBlock;
class UCanvasPanel;
class UVerticalBox;

// 시작 버튼 클릭 델리게이트 (PlayerController에서 바인딩)
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnMainMenuStartClicked);

/**
 * AOS 메인 메뉴 위젯
 * 게임 시작 및 종료 버튼을 제공하는 메인 메뉴 UI
 * 위젯을 C++에서 동적 생성 (BindWidget 불필요)
 */
UCLASS()
class TDPROJECT_API UAOSMainMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual bool Initialize() override;

	// PlayerController에서 구독: 시작 버튼 클릭 이벤트
	UPROPERTY(BlueprintAssignable)
	FOnMainMenuStartClicked OnStartClicked;

	// GameState 준비 상태 → StatusText 갱신 (로컬/원격 이름 포함)
	UFUNCTION(BlueprintCallable)
	void UpdateReadyState(const FString& LocalName, bool bLocalReady,
	                      const FString& RemoteName, bool bRemoteReady);

protected:
	UPROPERTY()
	UButton* StartGameButton;

	UPROPERTY()
	UButton* ExitGameButton;

	// 준비 상태 텍스트 ("게임 시작을 눌러 주세요" / "Team1: 준비 완료 ✓ ..." 등)
	UPROPERTY()
	UTextBlock* StatusText;

	UFUNCTION()
	void OnStartGameClicked();

	UFUNCTION()
	void OnExitGameClicked();

private:
	void BuildUI();
};
