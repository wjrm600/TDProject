#include "AOSShopWidget.h"
#include "AOSGameState.h"
#include "AOSPlayerState.h"
#include "AOSPlayerController.h"
#include "AOSUIStyle.h"
#include "GAS/Data/AOSItemData.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/Border.h"
#include "Components/Image.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/WrapBox.h"
#include "Components/WrapBoxSlot.h"
#include "Engine/DataTable.h"
#include "Engine/World.h"

// ─────────────────────────────────────────────────────────────
// UAOSShopUnitButton (View1)
// ─────────────────────────────────────────────────────────────
void UAOSShopUnitButton::BuildButtonUI(const FText& UnitName, int32 /*OwnedItemCount*/)
{
	Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(),
		*FString::Printf(TEXT("ShopUnitBtn_%d"), UnitListIndex));
	Button->SetStyle(AOSUIStyle::SolidButtonStyle(AOSUIStyle::PanelRaised));
	WidgetTree->RootWidget = Button;
	Button->OnClicked.AddDynamic(this, &UAOSShopUnitButton::HandleClicked);

	Label = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(),
		*FString::Printf(TEXT("ShopUnitLabel_%d"), UnitListIndex));
	Label->SetText(UnitName);
	Label->SetJustification(ETextJustify::Center);
	FSlateFontInfo F = Label->GetFont();
	F.Size = 16;
	Label->SetFont(F);
	Label->SetColorAndOpacity(FSlateColor(AOSUIStyle::TextSlate));   // 라이트 버튼 대비
	Button->AddChild(Label);
}

void UAOSShopUnitButton::HandleClicked()
{
	if (Owner)
	{
		Owner->SelectUnit(UnitListIndex);
	}
}

// ─────────────────────────────────────────────────────────────
// UAOSShopItemButton (View2)
// ─────────────────────────────────────────────────────────────
void UAOSShopItemButton::BuildButtonUI(const FText& DisplayName)
{
	Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(),
		*FString::Printf(TEXT("ShopItemBtn_%s"), *RowName.ToString()));
	Button->SetStyle(AOSUIStyle::SolidButtonStyle(AOSUIStyle::PanelRaised));
	WidgetTree->RootWidget = Button;
	Button->OnClicked.AddDynamic(this, &UAOSShopItemButton::HandleClicked);

	Label = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(),
		*FString::Printf(TEXT("ShopItemLabel_%s"), *RowName.ToString()));
	const FString Star = bRecommended ? TEXT("★ ") : TEXT("");
	Label->SetText(FText::FromString(
		FString::Printf(TEXT("%s%s   (%d G)"), *Star, *DisplayName.ToString(), Cost)));
	FSlateFontInfo F = Label->GetFont();
	F.Size = 15;
	Label->SetFont(F);
	if (bRecommended)
	{
		Label->SetColorAndOpacity(FSlateColor(AOSUIStyle::Gold));   // ★추천 = 진한 골드
	}
	Button->AddChild(Label);
}

void UAOSShopItemButton::SetAffordable(bool bAffordable)
{
	if (Button)
	{
		Button->SetIsEnabled(bAffordable);
	}
	if (Label && !bRecommended)
	{
		Label->SetColorAndOpacity(FSlateColor(bAffordable
			? AOSUIStyle::TextSlate
			: AOSUIStyle::TextFaint));   // 구매가능=본문 / 불가=흐린 회색
	}
}

void UAOSShopItemButton::HandleClicked()
{
	if (Owner)
	{
		Owner->HandleBuy(UnitId, RowName);
	}
}

// ─────────────────────────────────────────────────────────────
// UAOSShopWidget
// ─────────────────────────────────────────────────────────────

AAOSGameState* UAOSShopWidget::GetAOSGameStateChecked() const
{
	return GetWorld() ? GetWorld()->GetGameState<AAOSGameState>() : nullptr;
}

EAOSTeam UAOSShopWidget::GetLocalTeam() const
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

UDataTable* UAOSShopWidget::LoadItemTable() const
{
	return LoadObject<UDataTable>(nullptr, ItemTablePath());
}

TSharedRef<SWidget> UAOSShopWidget::RebuildWidget()
{
	// WBP_Shop(Parent=UAOSShopWidget)이 있으면 RootWidget 이 이미 채워짐 → 폴백 skip.
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		BuildShopUI();
	}
	return Super::RebuildWidget();
}

void UAOSShopWidget::BuildShopUI()
{
	// WBP 가 트리를 저작했으면(RootWidget 존재) 폴백 생성 skip — 멱등.
	if (WidgetTree && WidgetTree->RootWidget) { return; }

	// 전체화면 반투명 배경 (팝업 dim + 입력 차단)
	RootBg = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("ShopRootBg"));
	RootBg->SetBrushColor(FLinearColor(AOSUIStyle::BgDeep.R, AOSUIStyle::BgDeep.G, AOSUIStyle::BgDeep.B, 0.86f));   // 다크 팝업 dim
	RootBg->SetPadding(FMargin(120.0f, 70.0f));
	WidgetTree->RootWidget = RootBg;

	// 팝업 패널 (텍스처 키트 9-slice — 없으면 흰 카드 폴백)
	PanelBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("ShopPanel"));
	PanelBorder->SetBrush(AOSUIStyle::SolidBrush(AOSUIStyle::PanelBase));   // 토큰 솔리드(장식 텍스처 제거)
	PanelBorder->SetPadding(FMargin(56.0f, 48.0f));
	RootBg->SetContent(PanelBorder);

	UVerticalBox* ContentBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("ShopContent"));
	PanelBorder->SetContent(ContentBox);

	// ── 헤더: [뒤로] 타이틀(Fill) | 타이머 | 골드 | [닫기] ──
	UHorizontalBox* Header = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("ShopHeader"));
	UVerticalBoxSlot* HeaderVS = ContentBox->AddChildToVerticalBox(Header);
	HeaderVS->SetPadding(FMargin(0, 0, 0, 12));
	HeaderVS->SetHorizontalAlignment(EHorizontalAlignment::HAlign_Fill);

	// 뒤로가기 버튼 (아이템 페이지에서만 표시)
	BackButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("ShopBack"));
	BackButton->SetStyle(AOSUIStyle::SolidButtonStyle(AOSUIStyle::PanelRaised));
	BackButton->OnClicked.AddDynamic(this, &UAOSShopWidget::OnBackClicked);
	{
		UTextBlock* BackLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ShopBackLabel"));
		BackLabel->SetText(FText::FromString(TEXT("◀ 뒤로")));
		FSlateFontInfo BF = BackLabel->GetFont(); BF.Size = 15; BackLabel->SetFont(BF);
		BackLabel->SetColorAndOpacity(FSlateColor(AOSUIStyle::TextSlate));
		BackButton->AddChild(BackLabel);
	}
	UHorizontalBoxSlot* BackHS = Header->AddChildToHorizontalBox(BackButton);
	BackHS->SetVerticalAlignment(EVerticalAlignment::VAlign_Center);
	BackHS->SetPadding(FMargin(0, 0, 12, 0));

	TitleText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ShopTitle"));
	TitleText->SetText(FText::FromString(TEXT("상점 — 유닛 선택")));
	FSlateFontInfo TF = TitleText->GetFont(); TF.Size = 26; TitleText->SetFont(TF);
	TitleText->SetColorAndOpacity(FSlateColor(AOSUIStyle::TextSlate));   // 라이트 배경 대비
	UHorizontalBoxSlot* TitleHS = Header->AddChildToHorizontalBox(TitleText);
	TitleHS->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	TitleHS->SetVerticalAlignment(EVerticalAlignment::VAlign_Center);

	TimerText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ShopTimer"));
	TimerText->SetText(FText::FromString(TEXT("남은 시간: --")));
	FSlateFontInfo TmF = TimerText->GetFont(); TmF.Size = 18; TimerText->SetFont(TmF);
	TimerText->SetColorAndOpacity(FSlateColor(AOSUIStyle::Info));
	UHorizontalBoxSlot* TimerHS = Header->AddChildToHorizontalBox(TimerText);
	TimerHS->SetVerticalAlignment(EVerticalAlignment::VAlign_Center);
	TimerHS->SetPadding(FMargin(0, 0, 18, 0));

	GoldText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ShopGold"));
	GoldText->SetText(FText::FromString(TEXT("골드: 0")));
	FSlateFontInfo GF = GoldText->GetFont(); GF.Size = 18; GoldText->SetFont(GF);
	GoldText->SetColorAndOpacity(FSlateColor(AOSUIStyle::Gold));   // 진한 골드(라이트 대비)
	UHorizontalBoxSlot* GoldHS = Header->AddChildToHorizontalBox(GoldText);
	GoldHS->SetVerticalAlignment(EVerticalAlignment::VAlign_Center);
	GoldHS->SetPadding(FMargin(0, 0, 18, 0));

	CloseButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("ShopClose"));
	CloseButton->SetStyle(AOSUIStyle::SolidButtonStyle(AOSUIStyle::PanelRaised));
	CloseButton->OnClicked.AddDynamic(this, &UAOSShopWidget::OnCloseClicked);
	{
		UTextBlock* CloseLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ShopCloseLabel"));
		CloseLabel->SetText(FText::FromString(TEXT(" ✕ 닫기 ")));
		FSlateFontInfo CF = CloseLabel->GetFont(); CF.Size = 15; CloseLabel->SetFont(CF);
		CloseLabel->SetColorAndOpacity(FSlateColor(AOSUIStyle::TextSlate));
		CloseButton->AddChild(CloseLabel);
	}
	UHorizontalBoxSlot* CloseHS = Header->AddChildToHorizontalBox(CloseButton);
	CloseHS->SetVerticalAlignment(EVerticalAlignment::VAlign_Center);

	// 헤더 아래 장식 디바이더 (텍스처 키트 — 없으면 얇은 라인 폴백)
	HeaderDivider = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("ShopHeaderDivider"));
	HeaderDivider->SetBrush(AOSUIStyle::SolidBrush(AOSUIStyle::Border));   // 토큰 솔리드 얇은 라인
	HeaderDivider->SetDesiredSizeOverride(FVector2D(768.f, 2.f));
	UVerticalBoxSlot* DividerVS = ContentBox->AddChildToVerticalBox(HeaderDivider);
	DividerVS->SetPadding(FMargin(0, 0, 0, 10));
	DividerVS->SetHorizontalAlignment(EHorizontalAlignment::HAlign_Center);

	// ── View1: 유닛 선택 (WrapBox) ──
	UnitPickerBox = WidgetTree->ConstructWidget<UWrapBox>(UWrapBox::StaticClass(), TEXT("ShopUnitPicker"));
	UnitPickerBox->SetInnerSlotPadding(FVector2D(10.0f, 10.0f));
	UVerticalBoxSlot* PickerVS = ContentBox->AddChildToVerticalBox(UnitPickerBox);
	PickerVS->SetPadding(FMargin(0, 8, 0, 0));
	PickerVS->SetHorizontalAlignment(EHorizontalAlignment::HAlign_Fill);

	// ── View2: 아이템 페이지 (VerticalBox) ──
	ItemStoreBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("ShopItemStore"));
	UVerticalBoxSlot* StoreVS = ContentBox->AddChildToVerticalBox(ItemStoreBox);
	StoreVS->SetPadding(FMargin(0, 8, 0, 0));
	StoreVS->SetHorizontalAlignment(EHorizontalAlignment::HAlign_Fill);

	// 평소엔 팝업 자체를 숨김
	SetVisibility(ESlateVisibility::Collapsed);
}

void UAOSShopWidget::OpenForUnits(const TArray<FAOSShopUnit>& InUnits)
{
	Units = InUnits;

	// 헤더 버튼 바인딩 (WBP/폴백 공용 — WBP 경로에선 BuildShopUI 폴백이 스킵되므로 여기서 보장, 중복 방지)
	if (BackButton && !BackButton->OnClicked.IsAlreadyBound(this, &UAOSShopWidget::OnBackClicked))
	{
		BackButton->OnClicked.AddDynamic(this, &UAOSShopWidget::OnBackClicked);
	}
	if (CloseButton && !CloseButton->OnClicked.IsAlreadyBound(this, &UAOSShopWidget::OnCloseClicked))
	{
		CloseButton->OnClicked.AddDynamic(this, &UAOSShopWidget::OnCloseClicked);
	}

	// 골드 델리게이트 구독 (1회)
	if (!bSubscribed)
	{
		if (AAOSGameState* GS = GetAOSGameStateChecked())
		{
			if (!GS->OnTeamGoldChanged.IsAlreadyBound(this, &UAOSShopWidget::OnTeamGoldChanged))
			{
				GS->OnTeamGoldChanged.AddDynamic(this, &UAOSShopWidget::OnTeamGoldChanged);
			}
			bSubscribed = true;
		}
	}

	// 유닛 선택 버튼 재생성
	for (UAOSShopUnitButton* B : UnitButtons)
	{
		if (B) { B->RemoveFromParent(); }
	}
	UnitButtons.Empty();

	if (UnitPickerBox)
	{
		UnitPickerBox->ClearChildren();

		if (Units.Num() == 0)
		{
			UTextBlock* Empty = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ShopEmpty"));
			Empty->SetText(FText::FromString(TEXT("배치된 캐릭터가 없습니다. 먼저 캐릭터를 라인에 배치하세요.")));
			Empty->SetColorAndOpacity(FSlateColor(AOSUIStyle::TextMuted));
			UnitPickerBox->AddChildToWrapBox(Empty);
		}
		else
		{
			for (int32 i = 0; i < Units.Num(); ++i)
			{
				UAOSShopUnitButton* UB = WidgetTree->ConstructWidget<UAOSShopUnitButton>(
					UAOSShopUnitButton::StaticClass(), *FString::Printf(TEXT("ShopUnit_%d"), i));
				UB->UnitListIndex = i;
				UB->Owner = this;
				const FText Name = Units[i].DisplayName.IsEmpty()
					? FText::FromString(FString::Printf(TEXT("유닛 %d"), i + 1))
					: Units[i].DisplayName;
				UB->BuildButtonUI(Name, -1);

				UWrapBoxSlot* WS = UnitPickerBox->AddChildToWrapBox(UB);
				if (WS) { WS->SetPadding(FMargin(4.0f)); }
				UnitButtons.Add(UB);
			}
		}
	}

	// 열자마자 남은 시간 즉시 표시 (이후엔 CharacterSelect 가 매 틱 UpdateTimer 호출)
	if (AAOSGameState* GS = GetAOSGameStateChecked())
	{
		UpdateTimer(GS->PreparationTimeRemaining);
	}

	ShowUnitPicker();
	SetVisibility(ESlateVisibility::Visible);
}

void UAOSShopWidget::CloseShop()
{
	SetVisibility(ESlateVisibility::Collapsed);
}

bool UAOSShopWidget::IsOpen() const
{
	return GetVisibility() != ESlateVisibility::Collapsed;
}

void UAOSShopWidget::ShowUnitPicker()
{
	CurrentUnitListIndex = -1;
	if (UnitPickerBox) { UnitPickerBox->SetVisibility(ESlateVisibility::Visible); }
	if (ItemStoreBox)  { ItemStoreBox->SetVisibility(ESlateVisibility::Collapsed); }
	if (BackButton)    { BackButton->SetVisibility(ESlateVisibility::Collapsed); }
	if (TitleText)     { TitleText->SetText(FText::FromString(TEXT("상점 — 유닛 선택"))); }
	RefreshGoldAndAffordability();
}

void UAOSShopWidget::SelectUnit(int32 UnitListIndex)
{
	ShowItemStore(UnitListIndex);
}

void UAOSShopWidget::ShowItemStore(int32 UnitListIndex)
{
	if (!Units.IsValidIndex(UnitListIndex))
	{
		return;
	}
	CurrentUnitListIndex = UnitListIndex;
	const FAOSShopUnit& Unit = Units[UnitListIndex];

	RebuildItemButtons(Unit);

	if (UnitPickerBox) { UnitPickerBox->SetVisibility(ESlateVisibility::Collapsed); }
	if (ItemStoreBox)  { ItemStoreBox->SetVisibility(ESlateVisibility::Visible); }
	if (BackButton)    { BackButton->SetVisibility(ESlateVisibility::Visible); }
	if (TitleText)
	{
		const FText Name = Unit.DisplayName.IsEmpty()
			? FText::FromString(FString::Printf(TEXT("유닛 %d"), UnitListIndex + 1))
			: Unit.DisplayName;
		TitleText->SetText(FText::FromString(FString::Printf(TEXT("%s — 아이템"), *Name.ToString())));
	}
	RefreshGoldAndAffordability();
}

void UAOSShopWidget::RebuildItemButtons(const FAOSShopUnit& Unit)
{
	for (UAOSShopItemButton* B : ItemButtons)
	{
		if (B) { B->RemoveFromParent(); }
	}
	ItemButtons.Empty();
	if (!ItemStoreBox) { return; }
	ItemStoreBox->ClearChildren();

	UDataTable* DT = LoadItemTable();
	if (!DT)
	{
		UTextBlock* Err = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ShopItemErr"));
		Err->SetText(FText::FromString(TEXT("아이템 테이블(DT_Items)을 찾을 수 없습니다.")));
		Err->SetColorAndOpacity(FSlateColor(AOSUIStyle::BanRed));
		ItemStoreBox->AddChildToVerticalBox(Err);
		return;
	}

	UClass* UnitClass = Unit.CharacterClass; // 암시적 TSubclassOf → UClass*

	// 추천 먼저, 그 다음 일반 — 둘 다 테이블 순서 유지
	TArray<FName> Recommended;
	TArray<FName> Others;
	for (const FName& RN : DT->GetRowNames())
	{
		const FAOSItemRow* Row = DT->FindRow<FAOSItemRow>(RN, TEXT("RebuildItemButtons"));
		if (!Row) { continue; }

		bool bRec = false;
		if (UnitClass)
		{
			for (const TSubclassOf<AAOSCharacter>& RecClass : Row->RecommendedClasses)
			{
				UClass* RC = RecClass;
				if (RC && UnitClass->IsChildOf(RC)) { bRec = true; break; }
			}
		}
		(bRec ? Recommended : Others).Add(RN);
	}

	auto AddButton = [&](const FName& RN, bool bRec)
	{
		const FAOSItemRow* Row = DT->FindRow<FAOSItemRow>(RN, TEXT("RebuildItemButtons.Add"));
		if (!Row) { return; }

		UAOSShopItemButton* Btn = WidgetTree->ConstructWidget<UAOSShopItemButton>(
			UAOSShopItemButton::StaticClass(), *FString::Printf(TEXT("ShopItem_%s"), *RN.ToString()));
		Btn->UnitId = Unit.UnitId;
		Btn->RowName = RN;
		Btn->Cost = Row->Cost;
		Btn->bRecommended = bRec;
		Btn->Owner = this;
		Btn->BuildButtonUI(Row->DisplayName.IsEmpty() ? FText::FromName(RN) : Row->DisplayName);

		UVerticalBoxSlot* VS = ItemStoreBox->AddChildToVerticalBox(Btn);
		if (VS) { VS->SetPadding(FMargin(0, 3)); VS->SetHorizontalAlignment(EHorizontalAlignment::HAlign_Fill); }
		ItemButtons.Add(Btn);
	};

	for (const FName& RN : Recommended) { AddButton(RN, true); }
	for (const FName& RN : Others)      { AddButton(RN, false); }
}

void UAOSShopWidget::RefreshGoldAndAffordability()
{
	int32 Gold = 0;
	if (AAOSGameState* GS = GetAOSGameStateChecked())
	{
		Gold = GS->GetGold(GetLocalTeam());
	}
	if (GoldText)
	{
		GoldText->SetText(FText::FromString(FString::Printf(TEXT("골드: %d"), Gold)));
	}
	for (UAOSShopItemButton* Btn : ItemButtons)
	{
		if (Btn) { Btn->SetAffordable(Gold >= Btn->Cost); }
	}
}

void UAOSShopWidget::OnTeamGoldChanged(EAOSTeam Team, int32 /*NewGold*/)
{
	if (Team != GetLocalTeam()) { return; }
	RefreshGoldAndAffordability();
}

void UAOSShopWidget::UpdateTimer(float RemainingSeconds)
{
	if (!TimerText) { return; }
	const int32 Seconds = FMath::Max(0, FMath::CeilToInt(RemainingSeconds));
	TimerText->SetText(FText::FromString(FString::Printf(TEXT("남은 시간: %d초"), Seconds)));
	TimerText->SetColorAndOpacity(FSlateColor(Seconds <= 10
		? AOSUIStyle::Danger
		: AOSUIStyle::Info));
}

void UAOSShopWidget::HandleBuy(int32 UnitId, FName RowName)
{
	if (AAOSPlayerController* PC = Cast<AAOSPlayerController>(GetOwningPlayer()))
	{
		PC->Server_BuyItemForUnit(UnitId, RowName);
		UE_LOG(LogTemp, Log, TEXT("[Shop] 구매 요청 → UnitId=%d Row=%s"), UnitId, *RowName.ToString());
	}
}

void UAOSShopWidget::OnBackClicked()
{
	ShowUnitPicker();
}

void UAOSShopWidget::OnCloseClicked()
{
	CloseShop();
}

void UAOSShopWidget::NativeDestruct()
{
	if (bSubscribed)
	{
		if (AAOSGameState* GS = GetAOSGameStateChecked())
		{
			GS->OnTeamGoldChanged.RemoveDynamic(this, &UAOSShopWidget::OnTeamGoldChanged);
		}
		bSubscribed = false;
	}
	Super::NativeDestruct();
}
