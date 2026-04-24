#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/DragDropOperation.h"
#include "AOSGameMode.h"
#include "AOSCharacter.h"
#include "AOSCharacterSelectWidget.generated.h"

class UButton;
class UTextBlock;
class UCanvasPanel;
class UVerticalBox;
class UHorizontalBox;
class UWrapBox;
class UBorder;
class UImage;
class UAOSCharacterSelectWidget;
class UAOSCharacterDragDropOperation;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnStartRoundClicked);

// ─────────────────────────────────────────────────────────────
// 레인 드롭 슬롯 위젯 — 캐릭터 카드를 받아들이는 드롭 타겟
// ─────────────────────────────────────────────────────────────
UCLASS()
class TDPROJECT_API UAOSLaneSlotWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	EAOSLane OwnerLane = EAOSLane::Top;
	int32 SlotIndex = 0;

	UPROPERTY()
	UAOSCharacterSelectWidget* OwnerSelectWidget = nullptr;

	UPROPERTY()
	UBorder* SlotBorder;

	UPROPERTY()
	UTextBlock* SlotNameText;

	UPROPERTY()
	UButton* ClearButton;

	void BuildSlotUI(UWidgetTree* WT);
	void SetAssigned(TSubclassOf<AAOSCharacter> InClass, const FText& InName);
	void ClearAssignment();
	TSubclassOf<AAOSCharacter> GetAssignedClass() const { return AssignedClass; }

	virtual bool NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent,
		UDragDropOperation* InOperation) override;
	virtual bool NativeOnDragOver(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent,
		UDragDropOperation* InOperation) override;
	virtual void NativeOnDragLeave(const FDragDropEvent& InDragDropEvent,
		UDragDropOperation* InOperation) override;
	virtual void NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent,
		UDragDropOperation*& OutOperation) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry,
		const FPointerEvent& InMouseEvent) override;

private:
	TSubclassOf<AAOSCharacter> AssignedClass;

	UFUNCTION()
	void OnClearButtonClicked();
};

// ─────────────────────────────────────────────────────────────
// 캐릭터 카드 위젯 — 로스터에서 드래그하는 소스
// ─────────────────────────────────────────────────────────────
UCLASS()
class TDPROJECT_API UAOSCharacterCardWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	int32 RosterIndex = 0;
	TSubclassOf<AAOSCharacter> CharacterClass;

	UPROPERTY()
	UAOSCharacterSelectWidget* OwnerSelectWidget = nullptr;

	UPROPERTY()
	UTextBlock* NameText;

	UPROPERTY()
	UImage* PortraitImage;

	void SetupCard(int32 InIdx, TSubclassOf<AAOSCharacter> InClass, const FText& InName,
		UTexture2D* InPortrait, UAOSCharacterSelectWidget* InOwner, UWidgetTree* WT);

	virtual void NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent,
		UDragDropOperation*& OutOperation) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry,
		const FPointerEvent& InMouseEvent) override;
};

// ─────────────────────────────────────────────────────────────
// 캐릭터 선택/배치 위젯 (드래그앤드롭)
// ─────────────────────────────────────────────────────────────
UCLASS()
class TDPROJECT_API UAOSCharacterSelectWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual bool Initialize() override;

	// 라운드 번호 설정
	UFUNCTION(BlueprintCallable, Category = "AOS|UI")
	void SetRoundNumber(int32 RoundNum);

	// 준비 타이머 갱신 (PlayerController Tick에서 호출)
	UFUNCTION(BlueprintCallable, Category = "AOS|UI")
	void UpdatePreparationTimer(float RemainingSeconds);

	// 로스터 주입 (PlayerController::ShowCharacterSelect에서 호출)
	UFUNCTION(BlueprintCallable, Category = "AOS|UI")
	void InitializeWithRoster(const TArray<FCharacterRosterEntry>& Roster);

	// 레인별 배정 클래스 배열 반환 (PlayerController → RPC 전송 시 사용)
	UFUNCTION(BlueprintCallable, Category = "AOS|UI")
	TArray<TSubclassOf<AAOSCharacter>> GetLaneClasses(EAOSLane Lane) const;

	// 하위 호환: 라인별 배치 수 반환
	UFUNCTION(BlueprintCallable, Category = "AOS|UI")
	int32 GetLaneCount(EAOSLane Lane) const;

	// 하위 호환: 전체 배치 수 반환
	UFUNCTION(BlueprintCallable, Category = "AOS|UI")
	int32 GetTotalCount() const;

	// 하위 호환: 라인별 배치 수 초기화 (슬롯 비우기)
	UFUNCTION(BlueprintCallable, Category = "AOS|UI")
	void SetLaneCount(EAOSLane Lane, int32 Count);

	// 라운드 시작 클릭 델리게이트
	UPROPERTY(BlueprintAssignable, Category = "AOS|UI")
	FOnStartRoundClicked OnStartRoundClicked;

	// 드롭 이벤트 처리 (슬롯 위젯에서 위임받음)
	void HandleDropOnLaneSlot(EAOSLane Lane, int32 SlotIndex, UAOSCharacterDragDropOperation* Op);
	void HandleClearSlot(EAOSLane Lane, int32 SlotIndex);

protected:
	// 슬롯 위젯 6개 (3레인 × 2슬롯), 인덱스: LaneIndex*2 + SlotIndex
	UPROPERTY()
	TArray<UAOSLaneSlotWidget*> LaneSlotWidgets;

	// 캐릭터 카드 위젯 (로스터 크기만큼)
	UPROPERTY()
	TArray<UAOSCharacterCardWidget*> CharacterCardWidgets;

	// 캐시
	TArray<FCharacterRosterEntry> CachedRoster;

	UPROPERTY()
	UWrapBox* CardGrid;

	UPROPERTY()
	UTextBlock* TotalCountText;

	UPROPERTY()
	UTextBlock* TitleText;

	UPROPERTY()
	UTextBlock* TimerText;

	UPROPERTY()
	UButton* StartRoundButton;

	int32 MaxTotalCount = 5;
	int32 MaxPerLane = 2;

	// 하위 호환용 Count 캐시
	int32 TopLaneCount = 0;
	int32 MidLaneCount = 0;
	int32 BottomLaneCount = 0;

	UFUNCTION()
	void OnStartRoundButtonClicked();

private:
	void BuildUI();
	void UpdateCountsFromSlots();
	void RefreshLaneSlotDisplay(EAOSLane Lane, int32 SlotIndex);
	void RefreshTotalCountDisplay();
	int32 GetTotalAssignedCount() const;
};
