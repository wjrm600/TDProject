# 2026-02-18 전투 개선 및 HP 바 UI 구현

## 개요

이 날 작업에서는 사망/파괴 처리, 구조물 공격 기능, HP 바 UI 시스템을 구현했습니다.

---

## 1. 사망 / 파괴 처리 구현

### 캐릭터 사망 (AOSCharacter::OnCharacterDeath)

**문제**: 캐릭터가 체력이 0이 되어도 아무 처리 없이 계속 이동함

**구현:**
```cpp
void AAOSCharacter::OnCharacterDeath()
{
    // 1. 이동 중지
    GetCharacterMovement()->StopMovementImmediately();
    // 2. 콜리전 비활성화
    GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    // 3. 메시 숨기기
    GetMesh()->SetVisibility(false);
    // 4. HP 바 숨기기
    if (HealthBarComponent) HealthBarComponent->SetVisibility(false);
    // 5. GameMode에 알림
    if (AOSGameMode) AOSGameMode->OnCharacterDestroyed(this);
    // 6. 2초 후 Destroy (리스폰 없음)
    SetLifeSpan(2.0f);
}
```

### 구조물 파괴 (AOSStructure::OnStructureDestroyed)

**구현:**
```cpp
void AAOSStructure::OnStructureDestroyed()
{
    // 로그 출력 (팀, 라인, 타입, 위치)
    // 메시 숨기기
    MeshComponent->SetVisibility(false);
    // 감지 범위 비활성화 (더 이상 공격하지 않음)
    DetectionRange->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    // HP 바 숨기기
    HealthBarComponent->SetVisibility(false);
    // Tick 비활성화
    SetActorTickEnabled(false);
    // 타겟 해제
    CurrentTarget = nullptr;
}
```

### GameMode 연동 (AOSGameMode::OnCharacterDestroyed)

**구현:**
- Team1Characters / Team2Characters 배열에서 사망 캐릭터 제거
- 남은 캐릭터 수 로그 출력

### AI 컨트롤러 정리 (AOSAIController Tick)

**구현:**
- Tick에서 `!ControlledCharacter->IsAlive()` 확인
- 조건 충족 시: `StopMovement()`, `WaypointQueue.Empty()`, 레퍼런스 null 처리

---

## 2. 구조물 공격 기능 추가 (AttackStructure)

### 문제

캐릭터가 적 타워를 향해 이동은 했지만 공격하지 않음.
타워는 캐릭터를 공격하는데, 캐릭터는 타워를 그냥 지나치거나
타워 사거리 안에서 일방적으로 맞다가 사망.

### 원인

`UpdateAIBehavior()`에서 웨이포인트 구조물(타워/커맨드센터)에 도달해도
`MoveTowardsTarget()`만 호출하고 공격은 없었음.

### 해결

`AttackStructure()` 함수 추가:

```cpp
void AAOSAIController::AttackStructure(AAOSStructure* Structure, float DeltaTime)
{
    float Distance = FVector::Dist(ControlledCharacter->GetActorLocation(),
                                   Structure->GetActorLocation());
    if (Distance > ControlledCharacter->AttackRange)
    {
        // 사거리 밖: 구조물 방향으로 이동
        FVector Direction = (Structure->GetActorLocation() - ControlledCharacter->GetActorLocation()).GetSafeNormal();
        ControlledCharacter->AddMovementInput(Direction);
    }
    else
    {
        // 사거리 내: 이동 정지 후 공격 (쿨타임 1초, 데미지 10)
        StopMovement();
        if (CurrentAttackCooldown <= 0.0f)
        {
            Structure->ReceiveDamage(ControlledCharacter->AttackDamage);
            CurrentAttackCooldown = AttackCooldownDuration;
        }
    }
}
```

`UpdateAIBehavior()`에서 적 구조물 웨이포인트 처리:
```cpp
// 이전: MoveTowardsTarget()만 호출
// 이후: 웨이포인트가 적 구조물이면 AttackStructure() 호출
if (NextWaypoint && NextWaypoint->GetOwnerTeam() != ControlledCharacter->GetTeam())
{
    AttackStructure(NextWaypoint, DeltaTime);
}
else
{
    MoveTowardsTarget(DeltaTime);
}
```

---

## 3. 구조물 스탯 조정

| 속성 | 이전 값 | 변경 값 | 설명 |
|------|---------|---------|------|
| AttackRange | 1000.0f | 200.0f | 공격 사거리 축소 |
| DetectionRange (Sphere) | 1500.0f | 400.0f | 감지 범위 축소 |
| FindNearestEnemy DetectionRadius | 1500.0f | 400.0f | 코드 내 감지 반경도 동일하게 |

**이유**: 기본값이 너무 커서 타워가 너무 먼 거리에서 캐릭터를 공격함.
테스트 환경에서 타워가 사실상 라인 전체를 커버하는 문제 발생.

---

## 4. HP 바 UI 시스템 구현

### 신규 클래스: AOSHealthBarWidget (UI/AOSHealthBarWidget.h/cpp)

```cpp
UCLASS()
class TDPROJECT_API UAOSHealthBarWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable)
    void UpdateHealthPercent(float Percent);  // 0.0 ~ 1.0

    UFUNCTION(BlueprintCallable)
    void SetBarColor(FLinearColor Color);

protected:
    UPROPERTY(meta = (BindWidget))
    UProgressBar* HealthProgressBar;          // 이름이 정확히 일치해야 함
};
```

### AOSCharacter에 추가된 HP 바

```cpp
// 생성자
HealthBarComponent = CreateDefaultSubobject<UWidgetComponent>(TEXT("HealthBar"));
HealthBarComponent->SetupAttachment(RootComponent);
HealthBarComponent->SetRelativeLocation(FVector(0.0f, 0.0f, 100.0f));
HealthBarComponent->SetWidgetSpace(EWidgetSpace::Screen);
HealthBarComponent->SetDrawSize(FVector2D(100.0f, 10.0f));

// BeginPlay
// 팀 색상 적용: Team1=Red, Team2=Blue
```

### AOSStructure에 추가된 HP 바

```cpp
// 생성자
HealthBarComponent->SetRelativeLocation(FVector(0.0f, 0.0f, 250.0f));
HealthBarComponent->SetDrawSize(FVector2D(120.0f, 12.0f));

// HealthBarWidgetClass (EditAnywhere) - 에디터에서 WBP 할당 가능
```

### Build.cs 변경

```csharp
// 추가된 모듈
PublicDependencyModuleNames.AddRange(new string[] { ..., "SlateCore" });
PublicIncludePaths.Add("TDProject/AOS/UI");
```

### 에디터 작업 필요 사항

HP 바를 실제로 표시하려면 에디터에서 Widget Blueprint를 생성해야 합니다:

1. Content Browser → `Content/AOS/UI/` 폴더 생성
2. 우클릭 → User Interface → Widget Blueprint
3. 부모 클래스: `AOSHealthBarWidget`
4. 이름: `WBP_HealthBar`
5. 디자이너에서 ProgressBar 추가, 이름을 정확히 `HealthProgressBar`로 설정
6. BP_Character → HealthBarComponent → Widget Class → `WBP_HealthBar` 선택
7. 구조물 BP가 있다면 동일하게 `HealthBarWidgetClass` 설정

---

## 변경된 파일 목록

| 파일 | 변경 내용 |
|------|---------|
| `AOS/AOSCharacter.h` | HealthBarComponent, HealthBarWidget, UpdateHealthBar(), OnCharacterDeath() 추가 |
| `AOS/AOSCharacter.cpp` | HP 바 초기화, ReceiveDamage → UpdateHealthBar, OnCharacterDeath 구현 |
| `AOS/AOSAIController.h` | AttackStructure() 선언 |
| `AOS/AOSAIController.cpp` | Tick 사망 정리, UpdateAIBehavior 구조물 공격 처리, AttackStructure 구현 |
| `AOS/AOSStructure.h` | HealthBarComponent, HealthBarWidget, HealthBarWidgetClass, InitializeHealthBar() 추가 |
| `AOS/AOSStructure.cpp` | HP 바 초기화, ReceiveDamage → UpdateHealthBar, OnStructureDestroyed 구현, 스탯 조정 |
| `AOS/AOSGameMode.h` | OnCharacterDestroyed() 선언 |
| `AOS/AOSGameMode.cpp` | OnCharacterDestroyed 구현 (팀 배열에서 제거) |
| `AOS/UI/AOSHealthBarWidget.h` | **신규** - UUserWidget 기반 HP 바 위젯 |
| `AOS/UI/AOSHealthBarWidget.cpp` | **신규** - UpdateHealthPercent, SetBarColor 구현 |
| `TDProject.Build.cs` | SlateCore 모듈 추가, AOS/UI 인클루드 경로 추가 |

---

**작업일**: 2026-02-18
**커밋**: ea871e4 (캐릭터 사망/구조물 파괴 처리 및 구조물 공격 기능 구현)
