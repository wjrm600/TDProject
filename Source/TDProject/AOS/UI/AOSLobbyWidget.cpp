#include "AOSLobbyWidget.h"
#include "Components/TextBlock.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Blueprint/WidgetTree.h"

TSharedRef<SWidget> UAOSLobbyWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(
			UCanvasPanel::StaticClass(), TEXT("RootCanvas"));
		WidgetTree->RootWidget = Canvas;

		TitleText = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(), TEXT("TitleText"));
		TitleText->SetText(FText::FromString(TEXT("로비")));
		TitleText->SetColorAndOpacity(FLinearColor::White);
		Canvas->AddChild(TitleText);
		if (UCanvasPanelSlot* TitleSlot = Cast<UCanvasPanelSlot>(TitleText->Slot))
		{
			TitleSlot->SetAnchors(FAnchors(0.5f, 0.4f));
			TitleSlot->SetAlignment(FVector2D(0.5f, 0.5f));
			TitleSlot->SetSize(FVector2D(400.0f, 60.0f));
		}

		StatusText = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(), TEXT("StatusText"));
		StatusText->SetText(FText::FromString(TEXT("플레이어 대기 중...")));
		StatusText->SetColorAndOpacity(FLinearColor(0.8f, 0.8f, 0.8f, 1.0f));
		Canvas->AddChild(StatusText);
		if (UCanvasPanelSlot* StatusSlot = Cast<UCanvasPanelSlot>(StatusText->Slot))
		{
			StatusSlot->SetAnchors(FAnchors(0.5f, 0.5f));
			StatusSlot->SetAlignment(FVector2D(0.5f, 0.5f));
			StatusSlot->SetSize(FVector2D(400.0f, 40.0f));
		}
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
