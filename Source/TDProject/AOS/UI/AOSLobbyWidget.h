#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "AOSLobbyWidget.generated.h"

class UTextBlock;
class UButton;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnLobbyReadyClicked);

UCLASS()
class TDPROJECT_API UAOSLobbyWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "AOS|UI")
	void UpdatePlayerCount(int32 Connected, int32 Required);

	UFUNCTION(BlueprintCallable, Category = "AOS|UI")
	void UpdateReadyState(const FString& Team1Name, bool bTeam1Ready,
	                      const FString& Team2Name, bool bTeam2Ready);

	// 준비 버튼 클릭 시 브로드캐스트 (PlayerController에서 바인딩)
	UPROPERTY(BlueprintAssignable, Category = "AOS|UI")
	FOnLobbyReadyClicked OnReadyClicked;

protected:
	UPROPERTY(meta = (BindWidgetOptional))
	UTextBlock* TitleText;

	UPROPERTY(meta = (BindWidgetOptional))
	UTextBlock* StatusText;

	UPROPERTY(meta = (BindWidgetOptional))
	UTextBlock* Team1ReadyText;

	UPROPERTY(meta = (BindWidgetOptional))
	UTextBlock* Team2ReadyText;

	UPROPERTY(meta = (BindWidgetOptional))
	UButton* ReadyButton;

	// 하이브리드: WBP_Lobby(Parent=UAOSLobbyWidget) 있으면 디자이너 트리, 없으면 폴백 프레임 생성.
	virtual TSharedRef<SWidget> RebuildWidget() override;

	// 버튼 OnClicked 바인딩(WBP/폴백 공용 단일 경로 — 폴백에만 두면 WBP 경로에서 유실).
	virtual void NativeConstruct() override;

private:
	UFUNCTION()
	void OnReadyButtonClicked();
};
