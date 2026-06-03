// Slice 0: 상점 아이템 DataTable Row 정의.
// 아이템 = 비용 + 적용할 Infinite GameplayEffect(속성 증가). 라인 단위로 구매·누적.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "AOSItemData.generated.h"

class UGameplayEffect;
class UTexture2D;
class AAOSCharacter;

/**
 * 상점 아이템 1종을 정의하는 DataTable 행.
 *
 * 아이템 구매 → 해당 (팀,라인) 인벤토리에 RowName 추가 → 라운드 스폰 시
 * 그 라인의 모든 캐릭터에 StatEffect(Infinite GE)를 재적용 (라운드 리셋돼도 누적).
 *
 * DataTable 자산 약속:
 *   /Game/AOS/GAS/Data/DT_Items
 *
 * Row Name = 아이템 식별자 (예: "Sword", "Vitality"). 구매/인벤토리에서 이 이름으로 참조.
 */
USTRUCT(BlueprintType)
struct TDPROJECT_API FAOSItemRow : public FTableRowBase
{
	GENERATED_BODY()

	/** 상점 표시 이름 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AOS|Item")
	FText DisplayName;

	/** 구매 비용 (글로벌 골드) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AOS|Item", meta = (ClampMin = "0"))
	int32 Cost = 100;

	/** 착용 시 적용할 GameplayEffect — Infinite Duration + 속성 Additive 권장 (BP_GE_Item_*) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AOS|Item")
	TSubclassOf<UGameplayEffect> StatEffect;

	/** 상점 UI 아이콘 (선택) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AOS|Item")
	TObjectPtr<UTexture2D> Icon = nullptr;

	/** 짧은 설명 (툴팁용, 선택) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AOS|Item", meta = (MultiLine = true))
	FText Description;

	/**
	 * 추천 캐릭터 클래스 목록 (선택).
	 * 상점에서 이 클래스(또는 그 자식 BP) 캐릭터를 선택하면 이 아이템이 목록 상단에 정렬된다.
	 * 비워두면 추천 없음(기본 정렬).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AOS|Item")
	TArray<TSubclassOf<AAOSCharacter>> RecommendedClasses;
};
