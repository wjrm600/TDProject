#include "AOSHealthBarWidget.h"
#include "Components/ProgressBar.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Blueprint/WidgetTree.h"

TSharedRef<SWidget> UAOSHealthBarWidget::RebuildWidget()
{
	if (WidgetTree)
	{
		if (!WidgetTree->RootWidget)
		{
			// Case 1: C++ 클래스 직접 사용 — 위젯 트리 전체를 동적 생성
			UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(
				UCanvasPanel::StaticClass(), TEXT("RootCanvas"));
			WidgetTree->RootWidget = Canvas;

			HealthProgressBar = WidgetTree->ConstructWidget<UProgressBar>(
				UProgressBar::StaticClass(), TEXT("HealthProgressBar"));
			Canvas->AddChild(HealthProgressBar);

			if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(HealthProgressBar->Slot))
			{
				CanvasSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
				CanvasSlot->SetOffsets(FMargin(0.0f));
			}

			HealthProgressBar->SetPercent(1.0f);
			HealthProgressBar->SetFillColorAndOpacity(FLinearColor::Green);
		}
		else if (!HealthProgressBar)
		{
			// Case 2: WBP_HealthBar Blueprint 사용 중이지만 HealthProgressBar 위젯이 없는 경우
			// BindWidgetOptional이 null로 남을 때 → 기존 루트에 ProgressBar를 동적으로 추가
			UCanvasPanel* Canvas = Cast<UCanvasPanel>(WidgetTree->RootWidget);
			if (!Canvas)
			{
				// 루트가 CanvasPanel이 아닌 경우 새로 생성
				Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(
					UCanvasPanel::StaticClass(), TEXT("RootCanvas"));
				WidgetTree->RootWidget = Canvas;
			}

			HealthProgressBar = WidgetTree->ConstructWidget<UProgressBar>(
				UProgressBar::StaticClass(), TEXT("HealthProgressBar"));
			Canvas->AddChild(HealthProgressBar);

			if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(HealthProgressBar->Slot))
			{
				CanvasSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
				CanvasSlot->SetOffsets(FMargin(0.0f));
			}

			HealthProgressBar->SetPercent(1.0f);
			HealthProgressBar->SetFillColorAndOpacity(FLinearColor::Green);

			UE_LOG(LogTemp, Warning, TEXT("[HealthBarWidget] WBP_HealthBar에 HealthProgressBar 없음 → 동적 생성"));
		}
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
