#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "AOSHealthBarWidget.generated.h"

class UProgressBar;

/**
 * AOS HP 바 위젯
 * 캐릭터/구조물 머리 위에 표시되는 체력 바
 * Widget Blueprint 없이도 C++에서 자동으로 ProgressBar를 생성
 */
UCLASS()
class TDPROJECT_API UAOSHealthBarWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// HP 퍼센트 업데이트 (0.0 ~ 1.0)
	UFUNCTION(BlueprintCallable, Category = "AOS|UI")
	void UpdateHealthPercent(float Percent);

	// 바 색상 설정 (팀 색상)
	UFUNCTION(BlueprintCallable, Category = "AOS|UI")
	void SetBarColor(FLinearColor Color);

protected:
	// Widget Blueprint가 있으면 바인딩, 없으면 C++에서 생성
	UPROPERTY(meta = (BindWidgetOptional))
	UProgressBar* HealthProgressBar;

	// Widget Blueprint 없이 C++에서 위젯 트리 자동 생성
	virtual TSharedRef<SWidget> RebuildWidget() override;

	virtual void NativeDestruct() override;
};
