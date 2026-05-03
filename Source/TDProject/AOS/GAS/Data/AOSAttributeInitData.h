// GAS Phase 5+: AttributeSet 초기값 DataTable Row 정의
// 캐릭터 / 타워 / 커맨드센터 가 공통으로 사용하는 row 타입.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "AOSAttributeInitData.generated.h"

/**
 * UAOSAttributeSet 의 모든 속성 초기값을 담는 DataTable 행.
 *
 * 공통 row 타입 — 같은 구조체를 캐릭터/타워/CC 의 별도 DT 가 사용.
 * 구조물처럼 일부 속성을 안 쓰는 경우 그냥 default 값 그대로 두면 됨
 * (게임 로직이 사용하지 않는 속성은 무시).
 *
 * DataTable 자산 약속:
 *   /Game/AOS/GAS/Data/DT_CharacterAttributes      (Character 용)
 *   /Game/AOS/GAS/Data/DT_TowerAttributes          (Tower 용)
 *   /Game/AOS/GAS/Data/DT_CommandCenterAttributes  (CommandCenter 용)
 *
 * Row Name 약속:
 *   "Default" — 모든 캐릭터/구조물의 기본값
 *   캐릭터별 다른 값을 원하면 새 row 추가 후 BP 에서 RowName 만 변경
 */
USTRUCT(BlueprintType)
struct TDPROJECT_API FAOSAttributeInitRow : public FTableRowBase
{
	GENERATED_BODY()

	/** 시작 체력 (현재 체력) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AOS|Attributes")
	float Health = 100.f;

	/** 최대 체력 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AOS|Attributes")
	float MaxHealth = 100.f;

	/** 공격력 (기본 공격 1회당 데미지) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AOS|Attributes")
	float AttackPower = 10.f;

	/** 공격 사거리 (cm 단위) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AOS|Attributes")
	float AttackRange = 500.f;

	/** 공격 빈도 (1/sec, 쿨다운 = 1.0 / AttackSpeed) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AOS|Attributes")
	float AttackSpeed = 1.f;

	/** 이동 속도 (cm/sec — UE 기본값 600) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AOS|Attributes")
	float MoveSpeed = 600.f;
};
