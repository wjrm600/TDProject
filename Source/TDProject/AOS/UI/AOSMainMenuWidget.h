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
 * AOS 메인 메뉴 위젯 — WBP 하이브리드
 *  ▸ WBP_MainMenu(Parent=UAOSMainMenuWidget)가 있으면 디자이너 트리 사용(BindWidgetOptional 자동 바인딩),
 *    없으면 RebuildWidget 가드가 BuildFallbackFrame 으로 C++ 폴백 트리 생성. (BanPick/Lobby 패턴)
 *  ▸ 버튼 OnClicked 바인딩은 NativeConstruct 단일 경로(WBP/폴백 공용, IsAlreadyBound 중복 방지).
 */
UCLASS()
class TDPROJECT_API UAOSMainMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 하이브리드: WBP 있으면 디자이너 트리, 없으면 폴백 프레임 생성.
	virtual TSharedRef<SWidget> RebuildWidget() override;

	// 버튼 OnClicked 바인딩(WBP/폴백 공용 단일 경로).
	virtual void NativeConstruct() override;

	// PlayerController에서 구독: 시작 버튼 클릭 이벤트
	UPROPERTY(BlueprintAssignable)
	FOnMainMenuStartClicked OnStartClicked;

	// GameState 준비 상태 → StatusText 갱신 (로컬/원격 이름 포함)
	UFUNCTION(BlueprintCallable)
	void UpdateReadyState(const FString& LocalName, bool bLocalReady,
	                      const FString& RemoteName, bool bRemoteReady);

protected:
	// ── WBP 이름 계약 (BindWidgetOptional) — WBP_MainMenu 위젯 이름과 정확히 일치 시 자동 바인딩,
	//    없으면 BuildFallbackFrame 이 C++ 로 생성. ──
	UPROPERTY(meta = (BindWidgetOptional))
	UButton* StartGameButton = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	UButton* ExitGameButton = nullptr;

	// 준비 상태 텍스트 ("게임 시작을 눌러 주세요" / "Team1: 준비 완료 ✓ ..." 등)
	UPROPERTY(meta = (BindWidgetOptional))
	UTextBlock* StatusText = nullptr;

	UFUNCTION()
	void OnStartGameClicked();

	UFUNCTION()
	void OnExitGameClicked();

private:
	void BuildFallbackFrame();   // WBP 미저작 시 C++ 폴백 트리 (멱등 가드)
};
