#include "AOSHealthBarWidget.h"
#include "Components/ProgressBar.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Blueprint/WidgetTree.h"

TSharedRef<SWidget> UAOSHealthBarWidget::RebuildWidget()
{
	// Widget Blueprint가 없는 경우 C++에서 위젯 트리를 생성
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		// 루트 캔버스 패널 생성
		UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(
			UCanvasPanel::StaticClass(), TEXT("RootCanvas"));
		WidgetTree->RootWidget = Canvas;

		// ProgressBar 생성
		HealthProgressBar = WidgetTree->ConstructWidget<UProgressBar>(
			UProgressBar::StaticClass(), TEXT("HealthProgressBar"));
		Canvas->AddChild(HealthProgressBar);

		// 캔버스 전체를 채우도록 앵커/오프셋 설정
		if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(HealthProgressBar->Slot))
		{
			CanvasSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
			CanvasSlot->SetOffsets(FMargin(0.0f));
		}

		// 기본값: 100% 채움, 녹색
		HealthProgressBar->SetPercent(1.0f);
		HealthProgressBar->SetFillColorAndOpacity(FLinearColor::Green);
	}

	return Super::RebuildWidget();
}

void UAOSHealthBarWidget::UpdateHealthPercent(float Percent)
{
	if (HealthProgressBar)
	{
		HealthProgressBar->SetPercent(FMath::Clamp(Percent, 0.0f, 1.0f));
	}
}

void UAOSHealthBarWidget::SetBarColor(FLinearColor Color)
{
	if (HealthProgressBar)
	{
		HealthProgressBar->SetFillColorAndOpacity(Color);
	}
}

void UAOSHealthBarWidget::NativeDestruct()
{
	HealthProgressBar = nullptr;
	Super::NativeDestruct();
}
