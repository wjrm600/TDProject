// Slice 1: 미니맵 위젯 구현 (정적 배경 + 아이콘 오버레이 + 카메라 정렬/뷰박스/클릭이동).

#include "AOSMinimapWidget.h"
#include "AOSCharacter.h"
#include "AOSStructure.h"
#include "AOSPlayerController.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Border.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/Image.h"
#include "Brushes/SlateColorBrush.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"

namespace
{
	// HP 바와 동일 규칙: Team1=Red, Team2=Blue (미니맵 대비 위해 채도 ↑)
	FLinearColor TeamIconColor(EAOSTeam Team)
	{
		return (Team == EAOSTeam::Team1)
			? FLinearColor(1.0f, 0.18f, 0.18f, 1.0f)
			: FLinearColor(0.25f, 0.5f, 1.0f, 1.0f);
	}
}

void UAOSMinimapWidget::BuildUI()
{
	if (bBuilt || !WidgetTree)
	{
		return;
	}
	bBuilt = true;

	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("MiniRoot"));
	WidgetTree->RootWidget = Root;
	// 루트 캔버스는 화면 전체를 덮지만 self 는 통과 → 프레임(자식)만 클릭 캐치
	Root->SetVisibility(ESlateVisibility::SelfHitTestInvisible);

	FrameBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("MiniFrame"));
	FrameBorder->SetBrushColor(BackgroundColor);
	FrameBorder->SetPadding(FMargin(0.f)); // 패딩 0 → 아이콘 캔버스 로컬 = MinimapSize (클릭 좌표 정합)
	// 프레임만 hit-test 가능(클릭 캐처). 내부 콘텐츠는 통과.
	FrameBorder->SetVisibility(ESlateVisibility::Visible);

	// 화면 우하단 고정
	if (UCanvasPanelSlot* FS = Root->AddChildToCanvas(FrameBorder))
	{
		FS->SetAnchors(FAnchors(1.f, 1.f, 1.f, 1.f));
		FS->SetAlignment(FVector2D(1.f, 1.f));
		FS->SetSize(MinimapSize);
		FS->SetPosition(FVector2D(-ScreenMargin, -ScreenMargin));
	}

	ContentOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("MiniContent"));
	// 콘텐츠 전체 hit-test 통과 → 클릭은 뒤의 FrameBorder 가 받음
	ContentOverlay->SetVisibility(ESlateVisibility::HitTestInvisible);
	FrameBorder->SetContent(ContentOverlay);

	// 선택적 정적 배경 텍스처
	if (BackgroundTexture)
	{
		BackgroundImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("MiniBg"));
		BackgroundImage->SetBrushFromTexture(BackgroundTexture, false);
		if (UOverlaySlot* OS = ContentOverlay->AddChildToOverlay(BackgroundImage))
		{
			OS->SetHorizontalAlignment(HAlign_Fill);
			OS->SetVerticalAlignment(VAlign_Fill);
		}
	}

	// 아이콘/뷰박스 레이어 (배경 위)
	IconCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("MiniIcons"));
	if (UOverlaySlot* OS = ContentOverlay->AddChildToOverlay(IconCanvas))
	{
		OS->SetHorizontalAlignment(HAlign_Fill);
		OS->SetVerticalAlignment(VAlign_Fill);
	}

	// 장식 테두리 — 콘텐츠 맨 위에 "가운데 투명 프레임"을 덧댐 (클릭/아이콘 로직 불변).
	// FrameTexture 미설정 시 알려진 경로에서 자동 로드 → 텍스처만 임포트하면 적용됨.
	UTexture2D* FrameTex = FrameTexture;
	if (!FrameTex)
	{
		FrameTex = LoadObject<UTexture2D>(nullptr, TEXT("/Game/AOS/UI/Assets/T_MinimapFrame.T_MinimapFrame"));
	}
	if (FrameTex)
	{
		FrameImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("MiniFrameDeco"));
		FrameImage->SetBrushFromTexture(FrameTex, false);
		if (UOverlaySlot* OS = ContentOverlay->AddChildToOverlay(FrameImage))
		{
			OS->SetHorizontalAlignment(HAlign_Fill);
			OS->SetVerticalAlignment(VAlign_Fill);
		}
	}
}

void UAOSMinimapWidget::ShowMinimap()
{
	OwnerPC = Cast<AAOSPlayerController>(GetOwningPlayer());

	BuildUI();
	// 루트는 self 만 통과(자식=프레임은 클릭 받음) → 화면 전체를 막지 않음
	SetVisibility(ESlateVisibility::SelfHitTestInvisible);

	if (UWorld* World = GetWorld())
	{
		UpdateMinimap(); // 즉시 1회
		World->GetTimerManager().SetTimer(UpdateTimerHandle, this,
			&UAOSMinimapWidget::UpdateMinimap, UpdateInterval, /*bLoop*/ true);
	}
}

void UAOSMinimapWidget::HideMinimap()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(UpdateTimerHandle);
	}
	SetVisibility(ESlateVisibility::Collapsed);
}

bool UAOSMinimapWidget::ResolveWorldBounds()
{
	if (bBoundsValid)
	{
		return true;
	}

	UWorld* World = GetWorld();
	if (!World || !OwnerPC.IsValid())
	{
		return false; // 카메라 yaw 를 알아야 정렬 축을 확정 → PC 대기
	}

	TArray<AActor*> Structures;
	UGameplayStatics::GetAllActorsOfClass(World, AAOSStructure::StaticClass(), Structures);
	if (Structures.Num() < 2)
	{
		return false; // 복제 대기 — 다음 틱 재시도
	}

	// 화면축 정렬 기저 (카메라 yaw): screen-right=(-sinθ,cosθ), screen-up=(cosθ,sinθ)
	const float YawRad = FMath::DegreesToRadians(OwnerPC->GetCameraYaw() + ExtraYawDeg);
	const float Cz = FMath::Cos(YawRad);
	const float Sz = FMath::Sin(YawRad);
	AxisRight = FVector2D(-Sz, Cz);
	AxisUp = FVector2D(Cz, Sz);

	CachedStructures.Reset();
	RightMin = UpMin = FLT_MAX;
	RightMax = UpMax = -FLT_MAX;
	for (AActor* A : Structures)
	{
		AAOSStructure* S = Cast<AAOSStructure>(A);
		if (!S)
		{
			continue;
		}
		CachedStructures.Add(S);

		const FVector L = S->GetActorLocation();
		const FVector2D P(L.X, L.Y);
		const float R = FVector2D::DotProduct(P, AxisRight);
		const float U = FVector2D::DotProduct(P, AxisUp);
		RightMin = FMath::Min(RightMin, R);
		RightMax = FMath::Max(RightMax, R);
		UpMin = FMath::Min(UpMin, U);
		UpMax = FMath::Max(UpMax, U);
	}

	RightMin -= WorldPadding; RightMax += WorldPadding;
	UpMin -= WorldPadding; UpMax += WorldPadding;
	if (RightMax - RightMin < 1.f) { RightMax = RightMin + 1.f; }
	if (UpMax - UpMin < 1.f) { UpMax = UpMin + 1.f; }

	bBoundsValid = true;
	return true;
}

FVector2D UAOSMinimapWidget::WorldToLocal(const FVector& World) const
{
	const FVector2D P(World.X, World.Y);
	const float R = FVector2D::DotProduct(P, AxisRight);
	const float U = FVector2D::DotProduct(P, AxisUp);

	float LU = (RightMax > RightMin) ? (R - RightMin) / (RightMax - RightMin) : 0.5f;
	float LV = (UpMax > UpMin) ? (U - UpMin) / (UpMax - UpMin) : 0.5f;
	LU = FMath::Clamp(LU, 0.f, 1.f);
	LV = FMath::Clamp(LV, 0.f, 1.f);

	// 화면 up(LV 큰 값)이 미니맵 위 → 로컬 Y 는 아래로 증가하므로 (1-LV)
	// 콘텐츠를 프레임 안쪽으로 inset → 아이콘/뷰박스가 테두리에 안 닿아 프레임이 감싸는 느낌.
	const float InsetRatio = 0.10f; // ⚠ LocalToWorldGround 의 값과 반드시 동일하게 유지
	const float InX = MinimapSize.X * InsetRatio;
	const float InY = MinimapSize.Y * InsetRatio;
	return FVector2D(InX + LU * (MinimapSize.X - 2.f * InX),
	                 InY + (1.f - LV) * (MinimapSize.Y - 2.f * InY));
}

FVector UAOSMinimapWidget::LocalToWorldGround(const FVector2D& Local) const
{
	// WorldToLocal 의 inset 역변환 (InsetRatio 동일하게 유지)
	const float InsetRatio = 0.10f;
	const float InX = MinimapSize.X * InsetRatio;
	const float InY = MinimapSize.Y * InsetRatio;
	const float LU = FMath::Clamp((Local.X - InX) / (MinimapSize.X - 2.f * InX), 0.f, 1.f);
	const float LVDown = FMath::Clamp((Local.Y - InY) / (MinimapSize.Y - 2.f * InY), 0.f, 1.f);
	const float LV = 1.f - LVDown;

	const float R = FMath::Lerp(RightMin, RightMax, LU);
	const float U = FMath::Lerp(UpMin, UpMax, LV);

	// 직교 기저 → P = R*AxisRight + U*AxisUp
	const FVector2D P = R * AxisRight + U * AxisUp;
	return FVector(P.X, P.Y, 0.f);
}

void UAOSMinimapWidget::AddIcon(FLinearColor Color, float Size, const FVector2D& LocalPos)
{
	if (!IconCanvas || !WidgetTree)
	{
		return;
	}

	UImage* Icon = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
	if (!Icon)
	{
		return;
	}

	Icon->SetBrush(FSlateColorBrush(FLinearColor::White)); // 흰 브러시 × ColorAndOpacity = 팀 색
	Icon->SetColorAndOpacity(Color);

	if (UCanvasPanelSlot* S = IconCanvas->AddChildToCanvas(Icon))
	{
		S->SetAutoSize(false);
		S->SetSize(FVector2D(Size, Size));
		S->SetAlignment(FVector2D(0.5f, 0.5f)); // 위치 기준점 = 아이콘 중앙
		S->SetPosition(LocalPos);
	}
}

void UAOSMinimapWidget::AddRect(const FVector2D& TopLeftLocal, const FVector2D& SizeLocal, FLinearColor Color)
{
	if (!IconCanvas || !WidgetTree)
	{
		return;
	}

	UImage* Img = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
	if (!Img)
	{
		return;
	}

	Img->SetBrush(FSlateColorBrush(FLinearColor::White));
	Img->SetColorAndOpacity(Color);

	if (UCanvasPanelSlot* S = IconCanvas->AddChildToCanvas(Img))
	{
		S->SetAutoSize(false);
		S->SetAlignment(FVector2D(0.f, 0.f)); // 좌상단 기준
		S->SetSize(SizeLocal);
		S->SetPosition(TopLeftLocal);
	}
}

void UAOSMinimapWidget::DrawViewBox()
{
	if (!OwnerPC.IsValid() || !IconCanvas)
	{
		return;
	}

	int32 VX = 0, VY = 0;
	OwnerPC->GetViewportSize(VX, VY);
	if (VX <= 0 || VY <= 0)
	{
		return;
	}

	const FVector2D Corners[4] = {
		FVector2D(0.f, 0.f), FVector2D((float)VX, 0.f),
		FVector2D((float)VX, (float)VY), FVector2D(0.f, (float)VY)
	};

	FVector2D MappedMin(FLT_MAX, FLT_MAX);
	FVector2D MappedMax(-FLT_MAX, -FLT_MAX);
	int32 Hits = 0;
	for (int32 i = 0; i < 4; ++i)
	{
		FVector WorldPos, WorldDir;
		if (!OwnerPC->DeprojectScreenPositionToWorld(Corners[i].X, Corners[i].Y, WorldPos, WorldDir))
		{
			continue;
		}
		if (FMath::IsNearlyZero(WorldDir.Z))
		{
			continue;
		}
		const float T = -WorldPos.Z / WorldDir.Z;
		if (T <= 0.f)
		{
			continue; // 지평선 위(하늘) — 무시
		}
		const FVector Ground = WorldPos + T * WorldDir;
		const FVector2D L = WorldToLocal(Ground);
		MappedMin.X = FMath::Min(MappedMin.X, L.X);
		MappedMin.Y = FMath::Min(MappedMin.Y, L.Y);
		MappedMax.X = FMath::Max(MappedMax.X, L.X);
		MappedMax.Y = FMath::Max(MappedMax.Y, L.Y);
		++Hits;
	}

	if (Hits < 2)
	{
		return;
	}

	const float Th = ViewBoxThickness;
	const FVector2D Sz = MappedMax - MappedMin;
	if (Sz.X < Th || Sz.Y < Th)
	{
		return;
	}

	// 4변 테두리 (아이콘 위에 그려져 항상 보임)
	AddRect(FVector2D(MappedMin.X, MappedMin.Y), FVector2D(Sz.X, Th), ViewBoxColor);        // top
	AddRect(FVector2D(MappedMin.X, MappedMax.Y - Th), FVector2D(Sz.X, Th), ViewBoxColor);   // bottom
	AddRect(FVector2D(MappedMin.X, MappedMin.Y), FVector2D(Th, Sz.Y), ViewBoxColor);        // left
	AddRect(FVector2D(MappedMax.X - Th, MappedMin.Y), FVector2D(Th, Sz.Y), ViewBoxColor);   // right
}

void UAOSMinimapWidget::UpdateMinimap()
{
	UWorld* World = GetWorld();
	if (!World || !IconCanvas)
	{
		return;
	}
	if (!ResolveWorldBounds())
	{
		return; // 구조물/PC 복제 아직 — 다음 틱
	}

	IconCanvas->ClearChildren();

	// 구조물 (파괴 전까지) — CC 는 큰 아이콘
	for (AAOSStructure* S : CachedStructures)
	{
		if (!S || S->IsDestroyed())
		{
			continue;
		}
		const float Sz = (S->GetStructureType() == EStructureType::CommandCenter)
			? CommandCenterIconSize : StructureIconSize;
		AddIcon(TeamIconColor(S->GetOwnerTeam()), Sz, WorldToLocal(S->GetActorLocation()));
	}

	// 살아있는 캐릭터 (매 라운드 스폰/소멸 → 매 틱 재조회)
	TArray<AActor*> Chars;
	UGameplayStatics::GetAllActorsOfClass(World, AAOSCharacter::StaticClass(), Chars);
	for (AActor* A : Chars)
	{
		AAOSCharacter* Ch = Cast<AAOSCharacter>(A);
		if (!Ch || !Ch->IsAlive())
		{
			continue;
		}
		AddIcon(TeamIconColor(Ch->GetTeam()), CharacterIconSize, WorldToLocal(Ch->GetActorLocation()));
	}

	// 현재 카메라 가시 영역 (맨 위)
	DrawViewBox();
}

bool UAOSMinimapWidget::HandleMinimapPress(const FVector2D& ScreenPos)
{
	if (!bBoundsValid || !IconCanvas || !OwnerPC.IsValid())
	{
		return false;
	}

	// 화면 절대좌표 → 아이콘 캔버스 로컬 (WorldToLocal 출력과 동일 공간)
	const FVector2D Local = IconCanvas->GetCachedGeometry().AbsoluteToLocal(ScreenPos);
	if (Local.X < 0.f || Local.Y < 0.f || Local.X > MinimapSize.X || Local.Y > MinimapSize.Y)
	{
		return false; // 미니맵 밖
	}

	const FVector Ground = LocalToWorldGround(Local);
	OwnerPC->MoveCameraToGroundPoint(Ground);
	return true;
}

FReply UAOSMinimapWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton
		&& HandleMinimapPress(InMouseEvent.GetScreenSpacePosition()))
	{
		// 드래그 스크럽 위해 마우스 캡처
		FReply Reply = FReply::Handled();
		if (TSharedPtr<SWidget> Safe = GetCachedWidget())
		{
			Reply.CaptureMouse(Safe.ToSharedRef());
		}
		return Reply;
	}
	return FReply::Unhandled();
}

FReply UAOSMinimapWidget::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.IsMouseButtonDown(EKeys::LeftMouseButton)
		&& HandleMinimapPress(InMouseEvent.GetScreenSpacePosition()))
	{
		return FReply::Handled();
	}
	return FReply::Unhandled();
}

FReply UAOSMinimapWidget::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	// 캡처 해제 (눌렀던 게 우리였으면)
	return FReply::Handled().ReleaseMouseCapture();
}

FReply UAOSMinimapWidget::NativeOnTouchStarted(const FGeometry& InGeometry, const FPointerEvent& InGestureEvent)
{
	if (HandleMinimapPress(InGestureEvent.GetScreenSpacePosition()))
	{
		return FReply::Handled();
	}
	return FReply::Unhandled();
}

FReply UAOSMinimapWidget::NativeOnTouchMoved(const FGeometry& InGeometry, const FPointerEvent& InGestureEvent)
{
	if (HandleMinimapPress(InGestureEvent.GetScreenSpacePosition()))
	{
		return FReply::Handled();
	}
	return FReply::Unhandled();
}

void UAOSMinimapWidget::NativeDestruct()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(UpdateTimerHandle);
	}
	Super::NativeDestruct();
}
