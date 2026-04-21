# AIController 초기화 이슈 모음

**통합 문서**: AICONTROLLER_BEGINPLAY_TIMING + AICONTROLLER_POSSESSION_FIX + RUNTIME_SPAWN_AICONTROLLER_FIX

---

## 핵심 요약

AIController 관련 세 가지 이슈는 모두 **초기화 타이밍** 문제에서 비롯됩니다.

```
Unreal Engine 초기화 순서:
Pawn::BeginPlay() → AutoPossessAI → Controller::BeginPlay()

따라서 Controller::BeginPlay()에서 GetPawn()은 항상 유효.
하지만 런타임 스폰 시에는 BeginPlay가 다음 프레임에 호출됨.
```

---

## 이슈 1: BeginPlay에서 GetPawn() 처리

`AAOSAIController::BeginPlay()`에서 `GetPawn()`이 nullptr일 수 있는 상황과 그 처리.

### 정확한 Possession 순서

```
SpawnActor() → Pawn::BeginPlay() → AutoPossessAI → Controller::BeginPlay()
```

Controller::BeginPlay에서 GetPawn()은 **보통 유효**하지만, 런타임 스폰 직후에는 지연이 발생할 수 있습니다.

### 권장 구현

```cpp
void AAOSAIController::BeginPlay()
{
    Super::BeginPlay();

    ControlledCharacter = Cast<AAOSCharacter>(GetPawn());
    if (!ControlledCharacter)
    {
        // 다음 틱에 재시도
        GetWorld()->GetTimerManager().SetTimerForNextTick([this]()
        {
            if (this && this->IsValidLowLevel())
            {
                ControlledCharacter = Cast<AAOSCharacter>(GetPawn());
                if (ControlledCharacter)
                {
                    StartDeployment(ControlledCharacter->GetLane());
                }
            }
        });
        return;
    }

    StartDeployment(ControlledCharacter->GetLane());
}
```

---

## 이슈 2: AutoPossessAI 미작동

`AutoPossessAI = PlacedInWorld` 설정에도 AIController가 할당되지 않는 문제.

### 원인

Pawn 생성자에서 `AIControllerClass`를 명시적으로 설정해야 합니다.

### 해결 (AOSCharacter.cpp 생성자)

```cpp
AAOSCharacter::AAOSCharacter()
{
    // ...
    AIControllerClass = AAOSAIController::StaticClass();  // 필수
    AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
}
```

### Blueprint에서 확인

BP_Character Details 패널:
- `AutoPossessAI` = **PlacedInWorldOrSpawned**
- `AIControllerClass` = AOSAIController (자동 반영됨)

---

## 이슈 3: 런타임 스폰 시 AIController 미할당

`SpawnActor()`로 캐릭터 생성 직후 `DeployToLane()`을 호출하면 AIController가 아직 없는 문제.

### 원인

런타임 스폰 시 BeginPlay가 **다음 프레임**에 호출됩니다:

```
SpawnActor() 호출
  ↓ (즉시)
InitializeCharacter() → DeployToLane() ← AIController 없음!
  ↓ (다음 프레임)
BeginPlay() → AIController 할당
```

### 해결: SetTimerForNextTick 사용

```cpp
// AOSSpawnPoint::InitializeCharacter()
GetWorld()->GetTimerManager().SetTimerForNextTick([Character]()
{
    if (Character && Character->IsValidLowLevel())
    {
        Character->DeployToLane();  // BeginPlay 이후 실행 보장
    }
});
```

### Lambda 캡처 주의

```cpp
// 위험: 포인터 무효화 가능
[Character]() { Character->DoSomething(); }

// 안전: IsValidLowLevel 체크
[Character]() {
    if (Character && Character->IsValidLowLevel())
        Character->DoSomething();
}
```

---

## 디버그 체크리스트

AI가 작동하지 않을 때 순서대로 확인:

1. `AIControllerClass = AAOSAIController::StaticClass()` — 생성자에 있는가?
2. `AutoPossessAI = PlacedInWorldOrSpawned` — Blueprint에 설정됐는가?
3. `DeployToLane()` — SetTimerForNextTick으로 지연 호출하는가?
4. 콘솔 로그에서 `[AI Controller] Failed to possess` 검색

---

**최종 업데이트**: 2025-11-24
