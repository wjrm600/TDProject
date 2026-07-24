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
class UAOSShopWidget;

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
	void SetAssigned(TSubclassOf<AAOSCharacter> InClass, int32 InRosterIndex, const FText& InName);
	void ClearAssignment();
	TSubclassOf<AAOSCharacter> GetAssignedClass() const { return AssignedClass; }
	int32 GetAssignedRosterIndex() const { return AssignedRosterIndex; }

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
	int32 AssignedRosterIndex = -1; // 배정된 유닛의 로스터 인덱스 (= UnitId)

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

	// 하이브리드: WBP_CharacterSelect(Parent=UAOSCharacterSelectWidget)가 있으면 디자이너 트리 사용,
	// 없으면 RebuildWidget 가드가 BuildFallbackFrame 으로 C++ 트리 생성. (BanPick 패턴)
	virtual TSharedRef<SWidget> RebuildWidget() override;

	// WBP_Shop(Parent=UAOSShopWidget) 주입 — 없으면 LoadClass 폴백, 그것도 없으면 StaticClass.
	UPROPERTY(EditDefaultsOnly, Category = "AOS|UI")
	TSubclassOf<UUserWidget> ShopWidgetClass;

	// 라운드 번호 설정
	UFUNCTION(BlueprintCallable, Category = "AOS|UI")
	void SetRoundNumber(int32 RoundNum);

	// 준비 타이머 갱신 (PlayerController Tick에서 호출)
	UFUNCTION(BlueprintCallable, Category = "AOS|UI")
	void UpdatePreparationTimer(float RemainingSeconds);

	// 양 팀 준비 상태 표시 갱신 (PlayerController가 GameState OnRep_TeamReady 콜백에서 호출)
	UFUNCTION(BlueprintCallable, Category = "AOS|UI")
	void UpdateTeamReadyStatus(bool bTeam1Ready, bool bTeam2Ready);

	// 새 라운드 진입 시 로컬 준비 상태 리셋 (버튼 재활성화 + 텍스트 복원)
	UFUNCTION(BlueprintCallable, Category = "AOS|UI")
	void ResetReadyState();

	// 로스터 주입 (PlayerController::ShowCharacterSelect에서 호출)
	UFUNCTION(BlueprintCallable, Category = "AOS|UI")
	void InitializeWithRoster(const TArray<FCharacterRosterEntry>& Roster);

	// 레인별 배정 클래스 배열 반환 (PlayerController → RPC 전송 시 사용)
	UFUNCTION(BlueprintCallable, Category = "AOS|UI")
	TArray<TSubclassOf<AAOSCharacter>> GetLaneClasses(EAOSLane Lane) const;

	// 레인별 배정 UnitId(로스터 인덱스) 배열 — GetLaneClasses 와 평행 (유닛 귀속 아이템용)
	UFUNCTION(BlueprintCallable, Category = "AOS|UI")
	TArray<int32> GetLaneUnitIds(EAOSLane Lane) const;

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
	// ── WBP 이름 계약 (BindWidgetOptional) — WBP_CharacterSelect 위젯 이름과 정확히 일치 시 자동
	//    바인딩, 없으면 BuildFallbackFrame 이 C++ 로 생성. 동적 자식(카드/레인 슬롯)은 C++ 가 채움. ──

	// 슬롯 위젯 6개 (3레인 × 2슬롯), 인덱스: LaneIndex*2 + SlotIndex (C++ 가 레인 박스에 채움)
	UPROPERTY()
	TArray<UAOSLaneSlotWidget*> LaneSlotWidgets;

	// 캐릭터 카드 위젯 (로스터 크기만큼)
	UPROPERTY()
	TArray<UAOSCharacterCardWidget*> CharacterCardWidgets;

	// 캐시
	TArray<FCharacterRosterEntry> CachedRoster;

	// 레인 슬롯 컨테이너 (C++ 가 PopulateLanes 에서 드래그 슬롯 2개씩 채움)
	UPROPERTY(meta = (BindWidgetOptional))
	UVerticalBox* TopLaneBox = nullptr;
	UPROPERTY(meta = (BindWidgetOptional))
	UVerticalBox* MidLaneBox = nullptr;
	UPROPERTY(meta = (BindWidgetOptional))
	UVerticalBox* BotLaneBox = nullptr;

	// 로스터 카드 그리드 (C++ 가 InitializeWithRoster 에서 카드 채움)
	UPROPERTY(meta = (BindWidgetOptional))
	UWrapBox* CardGrid = nullptr;

	// 상점 팝업 — C++ 가 EnsureShopWidget 에서 생성/부착 (평소 Collapsed)
	UPROPERTY()
	UAOSShopWidget* ShopWidget = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	UTextBlock* TotalCountText = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	UTextBlock* TitleText = nullptr;

	// Slice 1: 지난 라운드 결과(라인 승패) 요약 — 첫 라운드엔 숨김
	UPROPERTY(meta = (BindWidgetOptional))
	UTextBlock* RoundResultText = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	UTextBlock* TimerText = nullptr;

	UPROPERTY(meta = (BindWidgetOptional))
	UButton* StartRoundButton = nullptr;

	// "라운드 준비" 버튼 내부의 텍스트 — 클릭 시 "준비 완료 ✓"로 변경
	UPROPERTY(meta = (BindWidgetOptional))
	UTextBlock* StartRoundButtonText = nullptr;

	// 상점 열기 버튼 (클릭 시 ShopWidget 팝업 표시)
	UPROPERTY(meta = (BindWidgetOptional))
	UButton* ShopButton = nullptr;

	// 양 팀 준비 상태 표시 텍스트 ("팀1: 준비완료 / 팀2: 대기중")
	UPROPERTY(meta = (BindWidgetOptional))
	UTextBlock* TeamReadyStatusText = nullptr;

	// 로컬 플레이어가 라운드 준비 버튼을 눌렀는지 여부 (재클릭 방지)
	bool bLocalPressedReady = false;

	int32 MaxTotalCount = 5;
	int32 MaxPerLane = 2;

	// 하위 호환용 Count 캐시
	int32 TopLaneCount = 0;
	int32 MidLaneCount = 0;
	int32 BottomLaneCount = 0;

	UFUNCTION()
	void OnStartRoundButtonClicked();

	// 상점 버튼 클릭 → 배치된 유닛 목록으로 상점 팝업 열기
	UFUNCTION()
	void OnShopButtonClicked();

private:
	void BuildFallbackFrame();          // WBP 미저작 시 C++ 폴백 트리 (멱등 가드)
	void PopulateLanes();               // 레인 박스에 드래그 슬롯 2개씩 채움 (바인딩/폴백 공용)
	void EnsureShopWidget();            // 상점 팝업 생성/부착 (바인딩/폴백 공용)
	void UpdateCountsFromSlots();
	void RefreshLaneSlotDisplay(EAOSLane Lane, int32 SlotIndex);
	void RefreshTotalCountDisplay();
	int32 GetTotalAssignedCount() const;

	// Slice 1: GameState 의 직전 라운드 결과를 읽어 로컬 팀 관점 승/패 요약 표시
	void UpdateRoundResult();
};
