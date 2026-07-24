#include "AOSLobbyWidget.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Blueprint/WidgetTree.h"
#include "AOSUIStyle.h"

TSharedRef<SWidget> UAOSLobbyWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(
			UCanvasPanel::StaticClass(), TEXT("RootCanvas"));
		WidgetTree->RootWidget = Canvas;

		// 제목
		TitleText = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(), TEXT("TitleText"));
		TitleText->SetText(FText::FromString(TEXT("로비")));
		TitleText->SetColorAndOpacity(AOSUIStyle::TextPrimary);
		Canvas->AddChild(TitleText);
		if (UCanvasPanelSlot* PanelSlot = Cast<UCanvasPanelSlot>(TitleText->Slot))
		{
			PanelSlot->SetAnchors(FAnchors(0.5f, 0.4f));
			PanelSlot->SetAlignment(FVector2D(0.5f, 0.5f));
			PanelSlot->SetSize(FVector2D(400.0f, 60.0f));
		}

		// 접속 현황 (N/2)
		StatusText = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(), TEXT("StatusText"));
		StatusText->SetText(FText::FromString(TEXT("플레이어 대기 중... (0/2)")));
		StatusText->SetColorAndOpacity(AOSUIStyle::TextMuted);
		Canvas->AddChild(StatusText);
		if (UCanvasPanelSlot* PanelSlot = Cast<UCanvasPanelSlot>(StatusText->Slot))
		{
			PanelSlot->SetAnchors(FAnchors(0.5f, 0.5f));
			PanelSlot->SetAlignment(FVector2D(0.5f, 0.5f));
			PanelSlot->SetSize(FVector2D(400.0f, 40.0f));
		}

		// Team1 준비 상태
		Team1ReadyText = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(), TEXT("Team1ReadyText"));
		Team1ReadyText->SetText(FText::FromString(TEXT("Team1: 대기 중...")));
		Team1ReadyText->SetColorAndOpacity(AOSUIStyle::TeamAccent(true));
		Canvas->AddChild(Team1ReadyText);
		if (UCanvasPanelSlot* PanelSlot = Cast<UCanvasPanelSlot>(Team1ReadyText->Slot))
		{
			PanelSlot->SetAnchors(FAnchors(0.5f, 0.57f));
			PanelSlot->SetAlignment(FVector2D(0.5f, 0.5f));
			PanelSlot->SetSize(FVector2D(400.0f, 35.0f));
		}

		// Team2 준비 상태
		Team2ReadyText = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(), TEXT("Team2ReadyText"));
		Team2ReadyText->SetText(FText::FromString(TEXT("Team2: 대기 중...")));
		Team2ReadyText->SetColorAndOpacity(AOSUIStyle::TeamAccent(false));
		Canvas->AddChild(Team2ReadyText);
		if (UCanvasPanelSlot* PanelSlot = Cast<UCanvasPanelSlot>(Team2ReadyText->Slot))
		{
			PanelSlot->SetAnchors(FAnchors(0.5f, 0.61f));
			PanelSlot->SetAlignment(FVector2D(0.5f, 0.5f));
			PanelSlot->SetSize(FVector2D(400.0f, 35.0f));
		}

		// 준비 버튼
		ReadyButton = WidgetTree->ConstructWidget<UButton>(
			UButton::StaticClass(), TEXT("ReadyButton"));
		ReadyButton->SetStyle(AOSUIStyle::SolidButtonStyle(AOSUIStyle::PanelRaised));
		ReadyButton->OnClicked.AddDynamic(this, &UAOSLobbyWidget::OnReadyButtonClicked);
		Canvas->AddChild(ReadyButton);
		if (UCanvasPanelSlot* PanelSlot = Cast<UCanvasPanelSlot>(ReadyButton->Slot))
		{
			PanelSlot->SetAnchors(FAnchors(0.5f, 0.68f));
			PanelSlot->SetAlignment(FVector2D(0.5f, 0.5f));
			PanelSlot->SetSize(FVector2D(200.0f, 60.0f));
		}

		// 준비 버튼 텍스트
		UTextBlock* ReadyButtonText = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(), TEXT("ReadyButtonText"));
		ReadyButtonText->SetText(FText::FromString(TEXT("준비")));
		ReadyButtonText->SetColorAndOpacity(AOSUIStyle::TextPrimary);
		ReadyButton->AddChild(ReadyButtonText);
	}

	return Super::RebuildWidget();
}

void UAOSLobbyWidget::UpdatePlayerCount(int32 Connected, int32 Required)
{
	if (StatusText)
	{
		FString StatusString = FString::Printf(
			TEXT("플레이어 대기 중... (%d/%d)"), Connected, Required);
		StatusText->SetText(FText::FromString(StatusString));
	}
}

void UAOSLobbyWidget::UpdateReadyState(const FString& Team1Name, bool bTeam1Ready,
                                        const FString& Team2Name, bool bTeam2Ready)
{
	const FString T1 = Team1Name.IsEmpty() ? TEXT("Team1") : Team1Name;
	const FString T2 = Team2Name.IsEmpty() ? TEXT("Team2") : Team2Name;

	if (Team1ReadyText)
	{
		Team1ReadyText->SetText(FText::FromString(
			bTeam1Ready
			? FString::Printf(TEXT("%s: 준비 완료 ✓"), *T1)
			: FString::Printf(TEXT("%s: 대기 중..."), *T1)));
		Team1ReadyText->SetColorAndOpacity(
			bTeam1Ready ? AOSUIStyle::Success : AOSUIStyle::TeamAccent(true));
	}

	if (Team2ReadyText)
	{
		Team2ReadyText->SetText(FText::FromString(
			bTeam2Ready
			? FString::Printf(TEXT("%s: 준비 완료 ✓"), *T2)
			: FString::Printf(TEXT("%s: 대기 중..."), *T2)));
		Team2ReadyText->SetColorAndOpacity(
			bTeam2Ready ? AOSUIStyle::Success : AOSUIStyle::TeamAccent(false));
	}
}

void UAOSLobbyWidget::OnReadyButtonClicked()
{
	OnReadyClicked.Broadcast();
}
