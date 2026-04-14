#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "AOSGameMode.h"
#include "AOSCharacterSelectWidget.generated.h"

class UButton;
class UTextBlock;
class UCanvasPanel;
class UVerticalBox;
class UHorizontalBox;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnStartRoundClicked);

/**
 * AOS 캐릭터 선택/배치 위젯
 * 라운드 준비 단계에서 각 라인에 배치할 캐릭터 수를 설정하는 UI
 * 위젯을 C++에서 동적 생성 (BindWidget 불필요)
 */
UCLASS()
class TDPROJECT_API UAOSCharacterSelectWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual bool Initialize() override;

	// 라운드 번호 설정
	UFUNCTION(BlueprintCallable, Category = "AOS|UI")
	void SetRoundNumber(int32 RoundNum);

	// 라인별 배치 수 가져오기
	UFUNCTION(BlueprintCallable, Category = "AOS|UI")
	int32 GetLaneCount(EAOSLane Lane) const;

	// 전체 배치 수 가져오기
	UFUNCTION(BlueprintCallable, Category = "AOS|UI")
	int32 GetTotalCount() const;

	// 라인별 배치 수 초기화
	UFUNCTION(BlueprintCallable, Category = "AOS|UI")
	void SetLaneCount(EAOSLane Lane, int32 Count);

	// 라운드 시작 클릭 델리게이트
	UPROPERTY(BlueprintAssignable, Category = "AOS|UI")
	FOnStartRoundClicked OnStartRoundClicked;

protected:
	// 라인별 UI 요소
	UPROPERTY()
	UTextBlock* TitleText;

	UPROPERTY()
	UTextBlock* TopLaneCountText;

	UPROPERTY()
	UTextBlock* MidLaneCountText;

	UPROPERTY()
	UTextBlock* BottomLaneCountText;

	UPROPERTY()
	UTextBlock* TotalCountText;

	UPROPERTY()
	UButton* TopMinusButton;

	UPROPERTY()
	UButton* TopPlusButton;

	UPROPERTY()
	UButton* MidMinusButton;

	UPROPERTY()
	UButton* MidPlusButton;

	UPROPERTY()
	UButton* BottomMinusButton;

	UPROPERTY()
	UButton* BottomPlusButton;

	UPROPERTY()
	UButton* StartRoundButton;

	// 라인별 배치 수
	int32 TopLaneCount = 2;
	int32 MidLaneCount = 2;
	int32 BottomLaneCount = 2;

	// 최대 총 배치 수
	int32 MaxTotalCount = 6;

	// 라인당 최소/최대
	int32 MinPerLane = 0;
	int32 MaxPerLane = 4;

	UFUNCTION()
	void OnTopMinusClicked();

	UFUNCTION()
	void OnTopPlusClicked();

	UFUNCTION()
	void OnMidMinusClicked();

	UFUNCTION()
	void OnMidPlusClicked();

	UFUNCTION()
	void OnBottomMinusClicked();

	UFUNCTION()
	void OnBottomPlusClicked();

	UFUNCTION()
	void OnStartRoundButtonClicked();

private:
	void BuildUI();
	void UpdateCountDisplays();
	void UpdateButtonStates();
	void ChangeLaneCount(EAOSLane Lane, int32 Delta);

	// 라인 행 생성 헬퍼
	UHorizontalBox* CreateLaneRow(UVerticalBox* Parent, const FString& LaneName,
		UTextBlock*& OutCountText, UButton*& OutMinusButton, UButton*& OutPlusButton, int32 RowIndex);
};
