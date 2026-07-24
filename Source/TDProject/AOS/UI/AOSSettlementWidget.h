#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "AOSGameMode.h"
#include "AOSSettlementWidget.generated.h"

class UTextBlock;
class UButton;
class UCanvasPanel;
class UVerticalBox;

/**
 * AOS 게임 결과(정산) 위젯 — WBP 하이브리드
 *  ▸ WBP_Settlement(Parent=UAOSSettlementWidget)가 있으면 디자이너 트리(BindWidgetOptional 자동 바인딩),
 *    없으면 RebuildWidget 가드가 BuildFallbackFrame 으로 C++ 폴백 트리 생성. (BanPick/Lobby 패턴)
 *  ▸ 버튼 OnClicked 바인딩은 NativeConstruct 단일 경로(WBP/폴백 공용, IsAlreadyBound 중복 방지).
 */
UCLASS()
class TDPROJECT_API UAOSSettlementWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 하이브리드: WBP 있으면 디자이너 트리, 없으면 폴백 프레임 생성.
	virtual TSharedRef<SWidget> RebuildWidget() override;

	// 버튼 OnClicked 바인딩(WBP/폴백 공용 단일 경로).
	virtual void NativeConstruct() override;

	UFUNCTION(BlueprintCallable, Category = "AOS|UI")
	void SetResult(EAOSTeam WinningTeam);

	UFUNCTION(BlueprintCallable, Category = "AOS|UI")
	void SetDraw();

protected:
	// ── WBP 이름 계약 (BindWidgetOptional) — WBP_Settlement 위젯 이름과 정확히 일치 시 자동 바인딩. ──
	UPROPERTY(meta = (BindWidgetOptional))
	UTextBlock* ResultText = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	UButton* ReturnToMainMenuButton = nullptr;

	UFUNCTION()
	void OnReturnClicked();

private:
	void BuildFallbackFrame();   // WBP 미저작 시 C++ 폴백 트리 (멱등 가드)
};
