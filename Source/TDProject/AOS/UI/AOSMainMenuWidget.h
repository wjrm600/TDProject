#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "AOSMainMenuWidget.generated.h"

class UButton;
class UTextBlock;
class UCanvasPanel;
class UVerticalBox;

/**
 * AOS 메인 메뉴 위젯
 * 게임 시작 및 종료 버튼을 제공하는 메인 메뉴 UI
 * 위젯을 C++에서 동적 생성 (BindWidget 불필요)
 */
UCLASS()
class TDPROJECT_API UAOSMainMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual bool Initialize() override;

protected:
	UPROPERTY()
	UButton* StartGameButton;

	UPROPERTY()
	UButton* ExitGameButton;

	UFUNCTION()
	void OnStartGameClicked();

	UFUNCTION()
	void OnExitGameClicked();

private:
	void BuildUI();
};
