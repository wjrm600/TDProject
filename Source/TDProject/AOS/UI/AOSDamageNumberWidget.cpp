// Slice 1: 플로팅 데미지 숫자 위젯 구현.

#include "AOSDamageNumberWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "GameFramework/PlayerController.h"

void UAOSDamageNumberWidget::SpawnDamageNumber(const UObject* WorldContext, float Damage,
	const FVector& WorldLocation, FLinearColor Color)
{
	if (!WorldContext || Damage <= 0.f)
	{
		return;
	}

	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	if (!World)
	{
		return;
	}

	// DS 는 뷰포트/렌더 파이프라인 없음 → 위젯 생성 금지
	if (World->GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	APlayerController* PC = World->GetFirstPlayerController();
	if (!PC || !PC->IsLocalController())
	{
		return;
	}

	UAOSDamageNumberWidget* W = CreateWidget<UAOSDamageNumberWidget>(PC, UAOSDamageNumberWidget::StaticClass());
	if (!W)
	{
		return;
	}

	// UMG 트리(RootWidget) 를 먼저 채운 뒤 뷰포트에 추가해야 슬레이트가 올바르게 빌드됨.
	W->InitDamageNumber(Damage, WorldLocation, Color);
	W->AddToViewport(/*ZOrder*/ 30);
}

void UAOSDamageNumberWidget::BuildUI()
{
	if (bBuilt || !WidgetTree)
	{
		return;
	}
	bBuilt = true;

	RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("DmgRoot"));
	WidgetTree->RootWidget = RootCanvas;

	DamageText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("DmgText"));
	if (RootCanvas && DamageText)
	{
		if (UCanvasPanelSlot* CSlot = RootCanvas->AddChildToCanvas(DamageText))
		{
			CSlot->SetAutoSize(true);
			// 위치 기준점을 텍스트 중앙으로 → 월드점에 정렬
			CSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		}
	}
}

void UAOSDamageNumberWidget::InitDamageNumber(float InDamage, const FVector& InWorldLocation, FLinearColor InColor)
{
	BuildUI();

	BaseWorldLocation = InWorldLocation;

	if (DamageText)
	{
		// 데미지가 클수록 글자 크게 (18~40)
		FSlateFontInfo F = DamageText->GetFont();
		F.Size = FMath::Clamp(18 + FMath::RoundToInt(InDamage * 0.15f), 18, 40);
		F.OutlineSettings.OutlineSize = 2;
		F.OutlineSettings.OutlineColor = FLinearColor(0.f, 0.f, 0.f, 0.9f);
		DamageText->SetFont(F);

		// 로케일 그룹핑(쉼표) 없이 정수로 표시
		DamageText->SetText(FText::FromString(FString::FromInt(FMath::RoundToInt(InDamage))));
		DamageText->SetColorAndOpacity(FSlateColor(InColor));
		DamageText->SetJustification(ETextJustify::Center);
	}

	SetVisibility(ESlateVisibility::HitTestInvisible);

	// 월드 타이머로 떠오름/페이드 구동 (Auto 틱 게이팅 회피)
	if (UWorld* World = GetWorld())
	{
		StartTimeSeconds = World->GetTimeSeconds();
		UpdateStep(); // 첫 프레임 위치 즉시 반영
		World->GetTimerManager().SetTimer(UpdateTimerHandle, this,
			&UAOSDamageNumberWidget::UpdateStep, 1.f / 60.f, /*bLoop*/ true);
	}
}

void UAOSDamageNumberWidget::UpdateStep()
{
	UWorld* World = GetWorld();
	APlayerController* PC = GetOwningPlayer();
	if (!World || !PC)
	{
		RemoveFromParent();
		return;
	}

	const float Elapsed = World->GetTimeSeconds() - StartTimeSeconds;
	if (Elapsed >= Lifetime)
	{
		World->GetTimerManager().ClearTimer(UpdateTimerHandle);
		RemoveFromParent();
		return;
	}

	// 월드 Z 로 떠오른 위치를 화면 좌표(DPI 보정된 뷰포트 로컬)로 투영
	const FVector CurLoc = BaseWorldLocation + FVector(0.f, 0.f, RiseWorldSpeed * Elapsed);

	FVector2D ScreenPos;
	const bool bOnScreen = UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(PC, CurLoc, ScreenPos, false);
	if (bOnScreen)
	{
		if (UCanvasPanelSlot* CSlot = DamageText ? Cast<UCanvasPanelSlot>(DamageText->Slot) : nullptr)
		{
			CSlot->SetPosition(ScreenPos);
		}

		// 마지막 40% 구간에서 페이드아웃 (0.6→1.0 구간을 1→0 으로 매핑)
		const float Alpha = Elapsed / Lifetime;
		const float Opacity = (Alpha < 0.6f)
			? 1.f
			: FMath::Clamp(1.f - (Alpha - 0.6f) / 0.4f, 0.f, 1.f);
		SetRenderOpacity(Opacity);
		SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	else
	{
		// 카메라 뒤/화면 밖 — 잠시 숨김 (수명 동안 위치가 다시 잡힐 수 있음)
		SetVisibility(ESlateVisibility::Hidden);
	}
}

void UAOSDamageNumberWidget::NativeDestruct()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(UpdateTimerHandle);
	}
	Super::NativeDestruct();
}
