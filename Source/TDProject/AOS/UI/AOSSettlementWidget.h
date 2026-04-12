#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "AOSGameMode.h"
#include "AOSSettlementWidget.generated.h"

class UTextBlock;
class UButton;
class UCanvasPanel;
class UVerticalBox;

/**
 * AOS 게임 결과(정산) 위젯
 * 승리/패배 결과를 표시하고 메인 메뉴로 복귀하는 UI
 * 위젯을 C++에서 동적 생성 (BindWidget 불필요)
 */
UCLASS()
class TDPROJECT_API UAOSSettlementWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;

	UFUNCTION(BlueprintCallable, Category = "AOS|UI")
	void SetResult(EAOSTeam WinningTeam);

protected:
	UPROPERTY()
	UTextBlock* ResultText;

	UPROPERTY()
	UButton* ReturnToMainMenuButton;

	UFUNCTION()
	void OnReturnClicked();

private:
	void BuildUI();
};
