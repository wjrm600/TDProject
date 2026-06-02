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
			ApplyOpaqueBackgroundStyle();
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
			ApplyOpaqueBackgroundStyle();

			UE_LOG(LogTemp, Warning, TEXT("[HealthBarWidget] WBP_HealthBar에 HealthProgressBar 없음 → 동적 생성"));
		}
	}

	return Super::RebuildWidget();
}

void UAOSHealthBarWidget::ApplyOpaqueBackgroundStyle()
{
	if (!HealthProgressBar)
	{
		return;
	}

	// 배경(드레인된 부분)과 채움 브러시를 불투명 단색 박스로 — 기본 ProgressBar 스타일의
	// 반투명/둥근 모서리 브러시가 World-space 위젯에서 깊이 정렬 깜빡임을 일으키는 것을 방지.
	FProgressBarStyle Style = HealthProgressBar->GetWidgetStyle();

	Style.BackgroundImage.DrawAs = ESlateBrushDrawType::Box;
	Style.BackgroundImage.TintColor = FSlateColor(FLinearColor(0.02f, 0.02f, 0.02f, 1.0f)); // 불투명 어두움

	Style.FillImage.DrawAs = ESlateBrushDrawType::Box;
	Style.FillImage.TintColor = FSlateColor(FLinearColor::White); // 실제 색은 SetFillColorAndOpacity 가 결정

	HealthProgressBar->SetWidgetStyle(Style);
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
