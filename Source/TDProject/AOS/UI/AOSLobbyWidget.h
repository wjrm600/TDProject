#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "AOSLobbyWidget.generated.h"

class UTextBlock;

UCLASS()
class TDPROJECT_API UAOSLobbyWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "AOS|UI")
	void UpdatePlayerCount(int32 Connected, int32 Required);

protected:
	UPROPERTY(meta = (BindWidgetOptional))
	UTextBlock* TitleText;

	UPROPERTY(meta = (BindWidgetOptional))
	UTextBlock* StatusText;

	virtual TSharedRef<SWidget> RebuildWidget() override;
};
