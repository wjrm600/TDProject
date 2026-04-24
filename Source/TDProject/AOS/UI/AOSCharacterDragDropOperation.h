#pragma once

#include "CoreMinimal.h"
#include "Blueprint/DragDropOperation.h"
#include "AOSGameMode.h"
#include "AOSCharacter.h"
#include "AOSCharacterDragDropOperation.generated.h"

/**
 * 캐릭터 카드를 레인 슬롯으로 드래그할 때 사용하는 페이로드
 */
UCLASS()
class TDPROJECT_API UAOSCharacterDragDropOperation : public UDragDropOperation
{
	GENERATED_BODY()

public:
	// 드래그 중인 캐릭터 클래스
	UPROPERTY()
	TSubclassOf<AAOSCharacter> CharacterClass;

	// 로스터 배열 내 인덱스
	UPROPERTY()
	int32 RosterIndex = -1;

	// true면 레인 슬롯에서 드래그된 것, false면 하단 카드 그리드에서 드래그
	UPROPERTY()
	bool bFromLaneSlot = false;

	// 소스 레인 슬롯 정보 (bFromLaneSlot=true일 때 유효)
	UPROPERTY()
	EAOSLane SourceLane = EAOSLane::Top;

	UPROPERTY()
	int32 SourceSlotIndex = -1;
};
