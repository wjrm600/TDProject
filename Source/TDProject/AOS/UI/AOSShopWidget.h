// 상점 UI (팝업): 준비 화면의 "상점" 버튼으로 열림.
//  View1) 배치된 유닛(최대 5) 선택  →  View2) 아이템 페이지(추천 먼저 + 뒤로가기/닫기)
// 아이템은 유닛(=로스터 인덱스)에 귀속 — 구매는 PlayerController::Server_BuyItemForUnit 로 전달.
// 헤더의 남은시간/골드는 두 뷰 모두에서 항상 표시.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "AOSGameMode.h" // EAOSTeam, EAOSLane
#include "AOSShopWidget.generated.h"

class UButton;
class UTextBlock;
class UBorder;
class UVerticalBox;
class UHorizontalBox;
class UWrapBox;
class UDataTable;
class AAOSCharacter;
class AAOSGameState;
class UAOSShopWidget;

// 상점에 표시할 "배치된 유닛" 1개 (CharacterSelect 가 채워서 넘김)
USTRUCT()
struct FAOSShopUnit
{
	GENERATED_BODY()

	UPROPERTY()
	int32 UnitId = -1; // 로스터 인덱스 (아이템 귀속 키)

	UPROPERTY()
	TSubclassOf<AAOSCharacter> CharacterClass = nullptr;

	UPROPERTY()
	FText DisplayName;
};

// ─────────────────────────────────────────────────────────────
// 유닛 선택 버튼 (View1) — 클릭 시 해당 유닛의 아이템 페이지로
// ─────────────────────────────────────────────────────────────
UCLASS()
class TDPROJECT_API UAOSShopUnitButton : public UUserWidget
{
	GENERATED_BODY()

public:
	int32 UnitListIndex = 0; // Owner->Units 인덱스

	UPROPERTY()
	UAOSShopWidget* Owner = nullptr;

	UPROPERTY()
	UButton* Button = nullptr;

	UPROPERTY()
	UTextBlock* Label = nullptr;

	void BuildButtonUI(const FText& UnitName, int32 OwnedItemCount);

private:
	UFUNCTION()
	void HandleClicked();
};

// ─────────────────────────────────────────────────────────────
// 아이템 구매 버튼 (View2) — 클릭 시 선택 유닛에 구매
// ─────────────────────────────────────────────────────────────
UCLASS()
class TDPROJECT_API UAOSShopItemButton : public UUserWidget
{
	GENERATED_BODY()

public:
	int32 UnitId = -1;
	FName RowName = NAME_None;
	int32 Cost = 0;
	bool bRecommended = false;

	UPROPERTY()
	UAOSShopWidget* Owner = nullptr;

	UPROPERTY()
	UButton* Button = nullptr;

	UPROPERTY()
	UTextBlock* Label = nullptr;

	void BuildButtonUI(const FText& DisplayName);
	void SetAffordable(bool bAffordable);

private:
	UFUNCTION()
	void HandleClicked();
};

// ─────────────────────────────────────────────────────────────
// 상점 팝업 위젯 — CharacterSelect 의 전체화면 오버레이 자식으로 배치(평소 Collapsed)
// ─────────────────────────────────────────────────────────────
UCLASS()
class TDPROJECT_API UAOSShopWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 자체 WidgetTree 로 정적 구조(배경/헤더/두 뷰 컨테이너) 1회 생성
	void BuildShopUI();

	// 상점 열기 — 배치 유닛 목록을 받아 유닛 선택 뷰 표시 + 가시화
	void OpenForUnits(const TArray<FAOSShopUnit>& InUnits);

	// 상점 닫기 (Collapsed)
	void CloseShop();

	bool IsOpen() const;

	// 준비창 남은 시간 갱신 (CharacterSelect 가 매 틱 전달) — 항상 표시
	void UpdateTimer(float RemainingSeconds);

	// 서브위젯 위임
	void SelectUnit(int32 UnitListIndex);     // UnitButton → 아이템 페이지
	void HandleBuy(int32 UnitId, FName RowName); // ItemButton → 서버 구매

	virtual void NativeDestruct() override;

protected:
	UPROPERTY()
	UBorder* RootBg = nullptr;

	UPROPERTY()
	UTextBlock* TitleText = nullptr;

	UPROPERTY()
	UTextBlock* TimerText = nullptr;

	UPROPERTY()
	UTextBlock* GoldText = nullptr;

	UPROPERTY()
	UButton* BackButton = nullptr;

	UPROPERTY()
	UButton* CloseButton = nullptr;

	UPROPERTY()
	UWrapBox* UnitPickerBox = nullptr;   // View1

	UPROPERTY()
	UVerticalBox* ItemStoreBox = nullptr; // View2

	UPROPERTY()
	TArray<UAOSShopUnitButton*> UnitButtons;

	UPROPERTY()
	TArray<UAOSShopItemButton*> ItemButtons;

	UPROPERTY()
	TArray<FAOSShopUnit> Units;

	int32 CurrentUnitListIndex = -1;

	UFUNCTION()
	void OnTeamGoldChanged(EAOSTeam Team, int32 NewGold);

	UFUNCTION()
	void OnBackClicked();

	UFUNCTION()
	void OnCloseClicked();

private:
	bool bSubscribed = false;

	void ShowUnitPicker();
	void ShowItemStore(int32 UnitListIndex);
	void RebuildItemButtons(const FAOSShopUnit& Unit);
	void RefreshGoldAndAffordability();

	EAOSTeam GetLocalTeam() const;
	AAOSGameState* GetAOSGameStateChecked() const;
	UDataTable* LoadItemTable() const;

	static const TCHAR* ItemTablePath() { return TEXT("/Game/AOS/GAS/Data/DT_Items"); }
};
