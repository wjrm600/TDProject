#include "AOSBanPickWidget.h"
#include "AOSGameState.h"
#include "AOSPlayerState.h"
#include "AOSPlayerController.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/WrapBox.h"
#include "Components/WrapBoxSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"

void UAOSBanPickWidget::BuildUI()
{
	if (bBuilt || !WidgetTree) return;
	bBuilt = true;

	// 루트: 반투명 어두운 백드롭 (Visible → 클릭 캐치)
	RootBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("BPRoot"));
	RootBorder->SetBrushColor(FLinearColor(0.02f, 0.02f, 0.05f, 0.92f));
	RootBorder->SetPadding(FMargin(24.f));
	RootBorder->SetVisibility(ESlateVisibility::Visible);
	WidgetTree->RootWidget = RootBorder;

	UVerticalBox* VB = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("BPVBox"));
	RootBorder->SetContent(VB);

	// 상태 텍스트 (현재 턴/밴or픽/타이머)
	StatusText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("BPStatus"));
	StatusText->SetText(FText::FromString(TEXT("드래프트 준비 중...")));
	StatusText->SetJustification(ETextJustify::Center);
	{
		FSlateFontInfo F = StatusText->GetFont();
		F.Size = 22;
		StatusText->SetFont(F);
	}
	if (UVerticalBoxSlot* S = VB->AddChildToVerticalBox(StatusText))
	{
		S->SetHorizontalAlignment(HAlign_Center);
		S->SetPadding(FMargin(0.f, 0.f, 0.f, 12.f));
	}

	// 카드 그리드 (로스터)
	CardGrid = WidgetTree->ConstructWidget<UWrapBox>(UWrapBox::StaticClass(), TEXT("BPGrid"));
	if (UVerticalBoxSlot* S = VB->AddChildToVerticalBox(CardGrid))
	{
		S->SetHorizontalAlignment(HAlign_Center);
		S->SetPadding(FMargin(0.f, 0.f, 0.f, 12.f));
	}

	// 팀별 밴/픽 목록 (하단 좌우)
	UHorizontalBox* HB = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("BPTeams"));
	if (UVerticalBoxSlot* S = VB->AddChildToVerticalBox(HB))
	{
		S->SetHorizontalAlignment(HAlign_Fill);
	}

	Team1Text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("BPTeam1"));
	Team1Text->SetColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.5f, 0.5f, 1.f)));
	if (UHorizontalBoxSlot* HS = HB->AddChildToHorizontalBox(Team1Text))
	{
		HS->SetHorizontalAlignment(HAlign_Left);
		HS->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}

	Team2Text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("BPTeam2"));
	Team2Text->SetColorAndOpacity(FSlateColor(FLinearColor(0.5f, 0.7f, 1.0f, 1.f)));
	if (UHorizontalBoxSlot* HS = HB->AddChildToHorizontalBox(Team2Text))
	{
		HS->SetHorizontalAlignment(HAlign_Right);
		HS->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}
}

void UAOSBanPickWidget::InitializeWithRoster(const TArray<FCharacterRosterEntry>& Roster)
{
	CachedRoster = Roster;
	BuildUI();
	if (!CardGrid) return;

	// 기존 카드 제거
	CardGrid->ClearChildren();
	CardBorders.Reset();
	CardImages.Reset();

	for (int32 i = 0; i < Roster.Num(); ++i)
	{
		UBorder* Card = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(),
			*FString::Printf(TEXT("BPCard_%d"), i));
		Card->SetVisibility(ESlateVisibility::HitTestInvisible); // 클릭은 루트가 히트테스트로 처리
		Card->SetPadding(FMargin(2.f));
		Card->SetBrushColor(FLinearColor(0.10f, 0.10f, 0.13f, 1.f));

		UVerticalBox* CB = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		Card->SetContent(CB);

		UImage* Img = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
		if (Roster[i].Portrait)
		{
			Img->SetBrushFromTexture(Roster[i].Portrait, false);
		}
		else
		{
			Img->SetBrush(FSlateColorBrush(FLinearColor(0.22f, 0.22f, 0.28f, 1.f)));
		}
		Img->SetDesiredSizeOverride(FVector2D(84.f, 84.f));
		CB->AddChildToVerticalBox(Img);

		UTextBlock* Name = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		Name->SetText(Roster[i].DisplayName);
		Name->SetJustification(ETextJustify::Center);
		{
			FSlateFontInfo F = Name->GetFont();
			F.Size = 11;
			Name->SetFont(F);
		}
		CB->AddChildToVerticalBox(Name);

		if (UWrapBoxSlot* WS = CardGrid->AddChildToWrapBox(Card))
		{
			WS->SetPadding(FMargin(4.f));
		}

		CardBorders.Add(Card);
		CardImages.Add(Img);
	}

	RefreshCards();
	RefreshStatus();
}

void UAOSBanPickWidget::NativeConstruct()
{
	Super::NativeConstruct();
	BuildUI();
	if (!bSubscribed)
	{
		if (AAOSGameState* GS = GetAOSGameState())
		{
			GS->OnDraftChanged.AddDynamic(this, &UAOSBanPickWidget::OnDraftChanged);
			bSubscribed = true;
		}
	}
}

void UAOSBanPickWidget::NativeDestruct()
{
	if (bSubscribed)
	{
		if (AAOSGameState* GS = GetAOSGameState())
		{
			GS->OnDraftChanged.RemoveDynamic(this, &UAOSBanPickWidget::OnDraftChanged);
		}
		bSubscribed = false;
	}
	Super::NativeDestruct();
}

void UAOSBanPickWidget::NativeTick(const FGeometry& MyGeometry, float DeltaTime)
{
	Super::NativeTick(MyGeometry, DeltaTime);
	// NativeTick 이 호출되면 타이머 갱신 (미호출 시엔 DraftTurnTimeRemaining 의 OnRep_Draft 가 갱신).
	RefreshStatus();
}

FReply UAOSBanPickWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	const FVector2D Abs = InMouseEvent.GetScreenSpacePosition();
	for (int32 i = 0; i < CardBorders.Num(); ++i)
	{
		if (CardBorders[i] && CardBorders[i]->GetCachedGeometry().IsUnderLocation(Abs))
		{
			HandleCardClicked(i);
			return FReply::Handled();
		}
	}
	return FReply::Unhandled();
}

void UAOSBanPickWidget::OnDraftChanged()
{
	RefreshCards();
	RefreshStatus();
}

void UAOSBanPickWidget::HandleCardClicked(int32 RosterIndex)
{
	AAOSGameState* GS = GetAOSGameState();
	if (!GS || GS->IsDraftComplete()) return;
	// 내 턴 + 가용 유닛만 (서버도 검증하지만 UX 로 선제 차단)
	if (GS->GetActiveDraftTeam() != GetLocalTeam()) return;
	if (!GS->IsUnitAvailableForDraft(RosterIndex)) return;

	if (AAOSPlayerController* PC = Cast<AAOSPlayerController>(GetOwningPlayer()))
	{
		PC->Server_DraftSelect(RosterIndex);
	}
}

void UAOSBanPickWidget::RefreshCards()
{
	AAOSGameState* GS = GetAOSGameState();
	if (!GS) return;
	const EAOSTeam Local = GetLocalTeam();
	const bool bMyTurn = !GS->IsDraftComplete() && (GS->GetActiveDraftTeam() == Local);

	for (int32 i = 0; i < CardImages.Num(); ++i)
	{
		if (!CardImages[i]) continue;
		FLinearColor Tint(1.f, 1.f, 1.f, 1.f);
		if (GS->IsUnitBanned(i))
		{
			Tint = FLinearColor(0.35f, 0.12f, 0.12f, 1.f);             // 밴 = 어두운 빨강
		}
		else if (GS->IsUnitPickedByTeam(i, EAOSTeam::Team1))
		{
			Tint = FLinearColor(1.0f, 0.45f, 0.45f, 1.f);             // T1 픽 = 빨강
		}
		else if (GS->IsUnitPickedByTeam(i, EAOSTeam::Team2))
		{
			Tint = FLinearColor(0.45f, 0.65f, 1.0f, 1.f);             // T2 픽 = 파랑
		}
		else if (!bMyTurn)
		{
			Tint = FLinearColor(0.5f, 0.5f, 0.5f, 1.f);               // 내 턴 아님 = 흐리게
		}
		CardImages[i]->SetColorAndOpacity(Tint);
	}
}

void UAOSBanPickWidget::RefreshStatus()
{
	AAOSGameState* GS = GetAOSGameState();
	if (!GS || !StatusText) return;

	if (GS->IsDraftComplete())
	{
		StatusText->SetText(FText::FromString(TEXT("드래프트 완료 — 라운드 준비로 이동합니다")));
	}
	else
	{
		const int32 ActiveTeam = static_cast<int32>(GS->GetActiveDraftTeam()) + 1;
		const FString Phase = GS->IsCurrentStepBan() ? TEXT("밴") : TEXT("픽");
		const int32 Secs = FMath::CeilToInt(GS->DraftTurnTimeRemaining);
		const bool bMine = (GS->GetActiveDraftTeam() == GetLocalTeam());
		StatusText->SetText(FText::FromString(FString::Printf(
			TEXT("팀%d %s 차례  (%ds)%s"), ActiveTeam, *Phase, Secs,
			bMine ? TEXT("  ← 당신의 차례!") : TEXT(""))));
	}

	auto BuildTeamLine = [this](const TArray<int32>& Bans, const TArray<int32>& Picks, const TCHAR* Label) -> FString
	{
		FString S = FString::Printf(TEXT("[%s]  밴: "), Label);
		for (int32 U : Bans)
		{
			S += (CachedRoster.IsValidIndex(U) ? CachedRoster[U].DisplayName.ToString() : FString::FromInt(U)) + TEXT(" ");
		}
		S += TEXT("\n      픽: ");
		for (int32 U : Picks)
		{
			S += (CachedRoster.IsValidIndex(U) ? CachedRoster[U].DisplayName.ToString() : FString::FromInt(U)) + TEXT(" ");
		}
		return S;
	};

	if (Team1Text)
	{
		Team1Text->SetText(FText::FromString(BuildTeamLine(GS->Team1BannedUnitIds, GS->Team1PickedUnitIds, TEXT("팀1"))));
	}
	if (Team2Text)
	{
		Team2Text->SetText(FText::FromString(BuildTeamLine(GS->Team2BannedUnitIds, GS->Team2PickedUnitIds, TEXT("팀2"))));
	}
}

AAOSGameState* UAOSBanPickWidget::GetAOSGameState() const
{
	return GetWorld() ? GetWorld()->GetGameState<AAOSGameState>() : nullptr;
}

EAOSTeam UAOSBanPickWidget::GetLocalTeam() const
{
	if (APlayerController* PC = GetOwningPlayer())
	{
		if (AAOSPlayerState* PS = PC->GetPlayerState<AAOSPlayerState>())
		{
			return PS->GetTeam();
		}
	}
	return EAOSTeam::Team1;
}
