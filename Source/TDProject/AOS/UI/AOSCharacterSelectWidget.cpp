#include "AOSCharacterSelectWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Spacer.h"

bool UAOSCharacterSelectWidget::Initialize()
{
	bool bSuccess = Super::Initialize();
	if (bSuccess)
	{
		BuildUI();
	}
	return bSuccess;
}

void UAOSCharacterSelectWidget::BuildUI()
{
	// Root CanvasPanel
	UCanvasPanel* RootPanel = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RootCanvas"));
	WidgetTree->RootWidget = RootPanel;

	// VerticalBox (centered)
	UVerticalBox* VBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("ContentBox"));
	UCanvasPanelSlot* VBoxSlot = RootPanel->AddChildToCanvas(VBox);
	VBoxSlot->SetAnchors(FAnchors(0.5f, 0.5f, 0.5f, 0.5f));
	VBoxSlot->SetAlignment(FVector2D(0.5f, 0.5f));
	VBoxSlot->SetAutoSize(true);

	// Title Text
	TitleText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("TitleText"));
	TitleText->SetText(FText::FromString(TEXT("Round 1 - 캐릭터 배치")));
	FSlateFontInfo TitleFont = TitleText->GetFont();
	TitleFont.Size = 36;
	TitleText->SetFont(TitleFont);
	TitleText->SetJustification(ETextJustify::Center);
	UVerticalBoxSlot* TitleSlot = VBox->AddChildToVerticalBox(TitleText);
	TitleSlot->SetPadding(FMargin(0, 0, 0, 30));
	TitleSlot->SetHorizontalAlignment(EHorizontalAlignment::HAlign_Center);

	// Separator line (using text)
	UTextBlock* SepTop = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("SepTop"));
	SepTop->SetText(FText::FromString(TEXT("----------------------------------------")));
	FSlateFontInfo SepFont = SepTop->GetFont();
	SepFont.Size = 14;
	SepTop->SetFont(SepFont);
	SepTop->SetJustification(ETextJustify::Center);
	UVerticalBoxSlot* SepTopSlot = VBox->AddChildToVerticalBox(SepTop);
	SepTopSlot->SetPadding(FMargin(0, 0, 0, 15));
	SepTopSlot->SetHorizontalAlignment(EHorizontalAlignment::HAlign_Center);

	// Lane rows
	CreateLaneRow(VBox, TEXT("Top Lane"), TopLaneCountText, TopMinusButton, TopPlusButton, 0);
	CreateLaneRow(VBox, TEXT("Mid Lane"), MidLaneCountText, MidMinusButton, MidPlusButton, 1);
	CreateLaneRow(VBox, TEXT("Bottom Lane"), BottomLaneCountText, BottomMinusButton, BottomPlusButton, 2);

	// Bind button events
	TopMinusButton->OnClicked.AddDynamic(this, &UAOSCharacterSelectWidget::OnTopMinusClicked);
	TopPlusButton->OnClicked.AddDynamic(this, &UAOSCharacterSelectWidget::OnTopPlusClicked);
	MidMinusButton->OnClicked.AddDynamic(this, &UAOSCharacterSelectWidget::OnMidMinusClicked);
	MidPlusButton->OnClicked.AddDynamic(this, &UAOSCharacterSelectWidget::OnMidPlusClicked);
	BottomMinusButton->OnClicked.AddDynamic(this, &UAOSCharacterSelectWidget::OnBottomMinusClicked);
	BottomPlusButton->OnClicked.AddDynamic(this, &UAOSCharacterSelectWidget::OnBottomPlusClicked);

	// Separator line (bottom)
	UTextBlock* SepBottom = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("SepBottom"));
	SepBottom->SetText(FText::FromString(TEXT("----------------------------------------")));
	SepBottom->SetFont(SepFont);
	SepBottom->SetJustification(ETextJustify::Center);
	UVerticalBoxSlot* SepBottomSlot = VBox->AddChildToVerticalBox(SepBottom);
	SepBottomSlot->SetPadding(FMargin(0, 15, 0, 10));
	SepBottomSlot->SetHorizontalAlignment(EHorizontalAlignment::HAlign_Center);

	// Total count text
	TotalCountText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("TotalCountText"));
	TotalCountText->SetText(FText::FromString(TEXT("")));
	FSlateFontInfo TotalFont = TotalCountText->GetFont();
	TotalFont.Size = 20;
	TotalCountText->SetFont(TotalFont);
	TotalCountText->SetJustification(ETextJustify::Center);
	UVerticalBoxSlot* TotalSlot = VBox->AddChildToVerticalBox(TotalCountText);
	TotalSlot->SetPadding(FMargin(0, 0, 0, 20));
	TotalSlot->SetHorizontalAlignment(EHorizontalAlignment::HAlign_Center);

	// Start Round Button
	StartRoundButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("StartRoundButton"));
	UVerticalBoxSlot* BtnSlot = VBox->AddChildToVerticalBox(StartRoundButton);
	BtnSlot->SetHorizontalAlignment(EHorizontalAlignment::HAlign_Center);
	BtnSlot->SetPadding(FMargin(0, 0, 0, 10));

	UTextBlock* BtnText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("StartRoundText"));
	BtnText->SetText(FText::FromString(TEXT("라운드 시작")));
	FSlateFontInfo BtnFont = BtnText->GetFont();
	BtnFont.Size = 24;
	BtnText->SetFont(BtnFont);
	StartRoundButton->AddChild(BtnText);
	StartRoundButton->OnClicked.AddDynamic(this, &UAOSCharacterSelectWidget::OnStartRoundButtonClicked);

	// Set initial display values
	UpdateCountDisplays();
	UpdateButtonStates();

	UE_LOG(LogTemp, Warning, TEXT("[CharacterSelect] UI 동적 생성 완료"));
}

UHorizontalBox* UAOSCharacterSelectWidget::CreateLaneRow(UVerticalBox* Parent, const FString& LaneName,
	UTextBlock*& OutCountText, UButton*& OutMinusButton, UButton*& OutPlusButton, int32 RowIndex)
{
	FString RowName = FString::Printf(TEXT("LaneRow_%d"), RowIndex);

	UHorizontalBox* HBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), *RowName);
	UVerticalBoxSlot* RowSlot = Parent->AddChildToVerticalBox(HBox);
	RowSlot->SetPadding(FMargin(20, 5, 20, 5));
	RowSlot->SetHorizontalAlignment(EHorizontalAlignment::HAlign_Center);

	// Lane Name Text
	FString LaneTextName = FString::Printf(TEXT("LaneName_%s"), *LaneName.Replace(TEXT(" "), TEXT("")));
	UTextBlock* LaneNameText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), *LaneTextName);
	LaneNameText->SetText(FText::FromString(LaneName));
	FSlateFontInfo LaneFont = LaneNameText->GetFont();
	LaneFont.Size = 20;
	LaneNameText->SetFont(LaneFont);
	UHorizontalBoxSlot* LaneNameSlot = HBox->AddChildToHorizontalBox(LaneNameText);
	LaneNameSlot->SetPadding(FMargin(0, 0, 30, 0));
	LaneNameSlot->SetVerticalAlignment(EVerticalAlignment::VAlign_Center);

	// Count display text
	FString CountTextName = FString::Printf(TEXT("Count_%s"), *LaneName.Replace(TEXT(" "), TEXT("")));
	OutCountText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), *CountTextName);
	OutCountText->SetText(FText::FromString(TEXT("2")));
	FSlateFontInfo CountFont = OutCountText->GetFont();
	CountFont.Size = 24;
	OutCountText->SetFont(CountFont);
	OutCountText->SetJustification(ETextJustify::Center);
	OutCountText->SetMinDesiredWidth(40.0f);
	UHorizontalBoxSlot* CountSlot = HBox->AddChildToHorizontalBox(OutCountText);
	CountSlot->SetPadding(FMargin(0, 0, 15, 0));
	CountSlot->SetVerticalAlignment(EVerticalAlignment::VAlign_Center);

	// Minus Button [-]
	FString MinusBtnName = FString::Printf(TEXT("MinusBtn_%s"), *LaneName.Replace(TEXT(" "), TEXT("")));
	OutMinusButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), *MinusBtnName);
	UHorizontalBoxSlot* MinusBtnSlot = HBox->AddChildToHorizontalBox(OutMinusButton);
	MinusBtnSlot->SetPadding(FMargin(0, 0, 5, 0));
	MinusBtnSlot->SetVerticalAlignment(EVerticalAlignment::VAlign_Center);

	FString MinusTextName = FString::Printf(TEXT("MinusText_%s"), *LaneName.Replace(TEXT(" "), TEXT("")));
	UTextBlock* MinusText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), *MinusTextName);
	MinusText->SetText(FText::FromString(TEXT(" - ")));
	FSlateFontInfo MinusFont = MinusText->GetFont();
	MinusFont.Size = 20;
	MinusText->SetFont(MinusFont);
	OutMinusButton->AddChild(MinusText);

	// Plus Button [+]
	FString PlusBtnName = FString::Printf(TEXT("PlusBtn_%s"), *LaneName.Replace(TEXT(" "), TEXT("")));
	OutPlusButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), *PlusBtnName);
	UHorizontalBoxSlot* PlusBtnSlot = HBox->AddChildToHorizontalBox(OutPlusButton);
	PlusBtnSlot->SetVerticalAlignment(EVerticalAlignment::VAlign_Center);

	FString PlusTextName = FString::Printf(TEXT("PlusText_%s"), *LaneName.Replace(TEXT(" "), TEXT("")));
	UTextBlock* PlusText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), *PlusTextName);
	PlusText->SetText(FText::FromString(TEXT(" + ")));
	FSlateFontInfo PlusFont = PlusText->GetFont();
	PlusFont.Size = 20;
	PlusText->SetFont(PlusFont);
	OutPlusButton->AddChild(PlusText);

	return HBox;
}

void UAOSCharacterSelectWidget::SetRoundNumber(int32 RoundNum)
{
	if (TitleText)
	{
		FString Title = FString::Printf(TEXT("Round %d - 캐릭터 배치"), RoundNum);
		TitleText->SetText(FText::FromString(Title));
	}
}

int32 UAOSCharacterSelectWidget::GetLaneCount(EAOSLane Lane) const
{
	switch (Lane)
	{
	case EAOSLane::Top:
		return TopLaneCount;
	case EAOSLane::Mid:
		return MidLaneCount;
	case EAOSLane::Bottom:
		return BottomLaneCount;
	default:
		return 0;
	}
}

int32 UAOSCharacterSelectWidget::GetTotalCount() const
{
	return TopLaneCount + MidLaneCount + BottomLaneCount;
}

void UAOSCharacterSelectWidget::SetLaneCount(EAOSLane Lane, int32 Count)
{
	Count = FMath::Clamp(Count, MinPerLane, MaxPerLane);

	switch (Lane)
	{
	case EAOSLane::Top:
		TopLaneCount = Count;
		break;
	case EAOSLane::Mid:
		MidLaneCount = Count;
		break;
	case EAOSLane::Bottom:
		BottomLaneCount = Count;
		break;
	}

	UpdateCountDisplays();
	UpdateButtonStates();
}

void UAOSCharacterSelectWidget::ChangeLaneCount(EAOSLane Lane, int32 Delta)
{
	int32 CurrentCount = GetLaneCount(Lane);
	int32 NewCount = CurrentCount + Delta;

	// 범위 체크 (라인당)
	if (NewCount < MinPerLane || NewCount > MaxPerLane)
	{
		return;
	}

	// 총합 체크
	int32 NewTotal = GetTotalCount() + Delta;
	if (NewTotal > MaxTotalCount || NewTotal < 0)
	{
		return;
	}

	SetLaneCount(Lane, NewCount);
}

void UAOSCharacterSelectWidget::UpdateCountDisplays()
{
	if (TopLaneCountText)
	{
		TopLaneCountText->SetText(FText::FromString(FString::Printf(TEXT("%d"), TopLaneCount)));
	}
	if (MidLaneCountText)
	{
		MidLaneCountText->SetText(FText::FromString(FString::Printf(TEXT("%d"), MidLaneCount)));
	}
	if (BottomLaneCountText)
	{
		BottomLaneCountText->SetText(FText::FromString(FString::Printf(TEXT("%d"), BottomLaneCount)));
	}
	if (TotalCountText)
	{
		FString TotalStr = FString::Printf(TEXT("총 배치: %d/%d"), GetTotalCount(), MaxTotalCount);
		TotalCountText->SetText(FText::FromString(TotalStr));
	}
}

void UAOSCharacterSelectWidget::UpdateButtonStates()
{
	int32 Total = GetTotalCount();
	bool bAtMax = (Total >= MaxTotalCount);

	// Minus buttons: disabled if lane count is at minimum
	if (TopMinusButton)
	{
		TopMinusButton->SetIsEnabled(TopLaneCount > MinPerLane);
	}
	if (MidMinusButton)
	{
		MidMinusButton->SetIsEnabled(MidLaneCount > MinPerLane);
	}
	if (BottomMinusButton)
	{
		BottomMinusButton->SetIsEnabled(BottomLaneCount > MinPerLane);
	}

	// Plus buttons: disabled if lane count is at max or total is at max
	if (TopPlusButton)
	{
		TopPlusButton->SetIsEnabled(!bAtMax && TopLaneCount < MaxPerLane);
	}
	if (MidPlusButton)
	{
		MidPlusButton->SetIsEnabled(!bAtMax && MidLaneCount < MaxPerLane);
	}
	if (BottomPlusButton)
	{
		BottomPlusButton->SetIsEnabled(!bAtMax && BottomLaneCount < MaxPerLane);
	}
}

void UAOSCharacterSelectWidget::OnTopMinusClicked()
{
	ChangeLaneCount(EAOSLane::Top, -1);
}

void UAOSCharacterSelectWidget::OnTopPlusClicked()
{
	ChangeLaneCount(EAOSLane::Top, 1);
}

void UAOSCharacterSelectWidget::OnMidMinusClicked()
{
	ChangeLaneCount(EAOSLane::Mid, -1);
}

void UAOSCharacterSelectWidget::OnMidPlusClicked()
{
	ChangeLaneCount(EAOSLane::Mid, 1);
}

void UAOSCharacterSelectWidget::OnBottomMinusClicked()
{
	ChangeLaneCount(EAOSLane::Bottom, -1);
}

void UAOSCharacterSelectWidget::OnBottomPlusClicked()
{
	ChangeLaneCount(EAOSLane::Bottom, 1);
}

void UAOSCharacterSelectWidget::OnStartRoundButtonClicked()
{
	UE_LOG(LogTemp, Warning, TEXT("[CharacterSelect] 라운드 시작 클릭 (Top:%d, Mid:%d, Bottom:%d, Total:%d)"),
		TopLaneCount, MidLaneCount, BottomLaneCount, GetTotalCount());
	OnStartRoundClicked.Broadcast();
}
