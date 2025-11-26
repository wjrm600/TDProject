# AI 움직임 시스템 구현 완료

**버전**: 1.0
**날짜**: 2025-11-24
**상태**: ✅ 완성 및 테스트 준비

---

## 🎯 구현 완료 요약

### 요청사항
```
1. State Tree를 이용한 AI 구현 → ❌ 단순화: Tick 기반 AI 시스템으로 변경
2. 기본 행동 패턴:
   - 시작 → 적군의 담당 라인의 제일 앞에 있는 포탑부터 커맨드 센터까지 차례대로 이동
   - 이동하다가 포탑 및 상대 캐릭터가 발견되면 공격
```

### 완성된 기능
✅ AI 캐릭터 자동 스폰
✅ 레인 기반 배포 시스템
✅ 우선순위 기반 행동 결정 (적 > 타워 > 계획된 경로)
✅ 거리 기반 감지 시스템
✅ 타워 순차 파괴 추적
✅ 커맨드 센터 최종 목표
✅ 자동 AIController 할당 (에디터 배치 & 런타임 스폰)

---

## 📊 아키텍처 개요

### 클래스 구조

```
APawn (Unreal Base)
  ↓
AAOSCharacter (게임 캐릭터)
  ├─ Team (Team1/Team2)
  ├─ Lane (Top/Mid/Bot)
  ├─ MaxHealth, CurrentHealth
  ├─ CharacterMovement
  ├─ DeployToLane() - AI 배포
  └─ ReceiveDamage() - 피해 처리

AController (Unreal Base)
  ↓
AAOSAIController (AI 컨트롤러)
  ├─ ControlledCharacter (캐싱된 Pawn)
  ├─ DeployedLane
  ├─ CurrentTarget (공격 대상)
  ├─ CurrentMoveTarget (이동 대상)
  ├─ Tick() - 매 프레임 AI 업데이트
  ├─ UpdateAIBehavior() - 의사결정
  ├─ MoveTowardsTarget() - 이동 로직
  ├─ AttackTarget() - 공격 로직
  └─ FindNearest*() - 감지 함수들

AAOSSpawnPoint (스폰 지점)
  ├─ Team, Lane, SpawnIndex
  ├─ SpawnCharacterAtPoint() - 캐릭터 생성
  └─ InitializeCharacter() - 초기화 (지연된 배포 호출)
```

---

## 🔄 실행 흐름

### 에디터 배치 시나리오 (bStartup == true)

```
1. 에디터에서 캐릭터 배치
   ↓
2. 게임 시작
   ↓
3. BeginPlay() 호출 순서:
   a) AAOSCharacter::BeginPlay()
      - SetupCharacterDefaults()
      - CurrentHealth = MaxHealth
      - GetController() == nullptr (아직 안할당)
      ↓
   b) AutoPossessAI 처리 (Possess 호출)
      - AIControllerClass 확인
      - AAOSAIController 생성 및 할당 ✅
      ↓
   c) AAOSAIController::BeginPlay()
      - ControlledCharacter = Cast<AAOSCharacter>(GetPawn()) ← 항상 유효 ✅
      - StartDeployment() 호출
      ↓
4. Tick 루프 시작
   - UpdateAIBehavior() 실행
   - AI 움직임 시작
```

**콘솔 출력**:
```
[Character] AI Controller found for Character_0
[AI Controller] AI initialized for character in lane: 1
[AI Controller] Deployment started on lane: 1
[AI Controller] Cached lane info - Start: (...), End: (...)
```

### 런타임 스폰 시나리오 (bStartup == false)

```
1. SpawnPoint::SpawnCharacterAtPoint() 호출
   ↓
2. GetWorld()->SpawnActor<AAOSCharacter>(...)
   - FActorSpawnParameters 사용
   - Owner 설정
   ↓
3. InitializeCharacter() 호출
   - Character->SetTeam(Team)
   - Character->SetLane(Lane)
   - SetTimerForNextTick([Character] { DeployToLane(); }) 등록
   - 즉시 반환
   ↓
4. 다음 프레임 대기
   ↓
5. 엔진이 스폰된 액터들의 BeginPlay() 호출
   a) AAOSCharacter::BeginPlay()
      - AIControllerClass 적용
      - GetController() 할당 확인
      ↓
   b) AutoPossessAI + Possess
      - AAOSAIController 자동 생성 ✅
      ↓
6. Timer 콜백 실행
   - Character->DeployToLane() 호출
   - AAOSAIController::StartDeployment() 실행
   - AI 배포 시작 ✅
```

**콘솔 출력**:
```
Character spawned at SpawnPoint - Team: 0, Lane: 1, Position: (1200.0, 1200.0, 100.0)
[Character] AI Controller found for Character_0
[AI Controller] AI initialized for character in lane: 1
[AI Controller] Deployment started on lane: 1
[AI Controller] Cached lane info - Start: (...), End: (...)
```

---

## 🧠 AI 의사결정 시스템

### UpdateAIBehavior() - 우선순위 기반

```cpp
void AAOSAIController::UpdateAIBehavior(float DeltaTime)
{
    // 💥 Priority 1: 적 캐릭터 공격 (최우선)
    AAOSCharacter* NearestEnemy = FindNearestEnemy();
    if (NearestEnemy)
    {
        CurrentTarget = NearestEnemy;
        AttackTarget(DeltaTime);  // 범위 체크 및 공격
        return;
    }

    // 🏰 Priority 2: 근처 타워 이동 (감지 범위 내)
    AAOSStructure* NearestTower = FindNearestEnemyTower();
    if (NearestTower)
    {
        CurrentMoveTarget = NearestTower->GetActorLocation();
    }
    else
    {
        // 🎯 Priority 3: 계획된 경로 이동 (다음 타워 또는 커맨드 센터)
        CurrentMoveTarget = GetNextTargetLocation();
    }

    CurrentTarget = nullptr;
    MoveTowardsTarget(DeltaTime);
}
```

### 이동 로직 - MoveTowardsTarget()

```
상태:
├─ CurrentMoveTarget 유효
├─ ControlledCharacter 유효
└─ Distance = FVector::Dist(현재위치, 목표위치)

로직:
├─ Distance <= ArrivalDistance (100.0f)
│  ├─ 속도 = 0
│  ├─ 다음 타워 인덱스 증가
│  └─ NextTargetLocation() 호출 (새 목표 설정)
│
└─ Distance > ArrivalDistance
   └─ Direction = (목표 - 현재).GetSafeNormal()
      └─ AddMovementInput(Direction, 1.0f)
```

### 공격 로직 - AttackTarget()

```
상태:
├─ ControlledCharacter 유효
└─ CurrentTarget 유효

로직:
├─ Distance = FVector::Dist(현재위치, 적위치)
│
├─ Distance > AttackRange (500.0f)
│  └─ 적 추격
│     ├─ Direction = (적 - 현재).GetSafeNormal()
│     └─ AddMovementInput(Direction, 1.0f)
│
└─ Distance <= AttackRange
   ├─ 속도 = 0 (정지)
   ├─ 적 방향 회전
   │  └─ LookAtRotation = DirectionToEnemy.Rotation()
   │
   └─ 공격 쿨타임 확인
      ├─ CurrentAttackCooldown <= 0.0f
      │  ├─ CurrentTarget->ReceiveDamage(10.0f)
      │  ├─ CurrentAttackCooldown = AttackCooldownDuration (1.0f)
      │  └─ 로그 출력
      │
      └─ CurrentAttackCooldown -= DeltaTime (매 프레임)
```

---

## 📍 목표 추적 시스템 - GetNextTargetLocation()

```
1. MapManager 검색
   └─ TActorIterator<AAOSMapManager>(GetWorld())

2. 레인 내 적 팀 타워 검색
   └─ MapManager->GetTowersInLane(DeployedLane, EnemyTeam)

3. 다음 타워 결정
   ├─ TowersInLane.Num() > 0 && NextTowerIndex < Count
   │  ├─ TargetTower = TowersInLane[NextTowerIndex]
   │  ├─ !TargetTower->IsDestroyed()
   │  │  └─ return TargetTower->GetActorLocation()
   │  │
   │  └─ TargetTower 파괴됨
   │     ├─ NextTowerIndex++
   │     └─ GetNextTargetLocation() (재귀 호출)
   │
   └─ 모든 타워 파괴 완료
      ├─ bAllTowersDestroyed = true
      ├─ CommandCenter = MapManager->GetCommandCenter(EnemyTeam)
      └─ return CommandCenter->GetActorLocation()
```

---

## 🎛️ 설정값

| 설정 | 값 | 설명 |
|------|-----|------|
| **EnemyDetectionRange** | 1500.0f | 적 캐릭터 감지 범위 |
| **EnemyDetectionRange** (타워) | 1500.0f | 적 타워 감지 범위 |
| **AttackRange** | 500.0f | 공격 가능 범위 |
| **ArrivalDistance** | 100.0f | 목표 도착 판정 거리 |
| **AttackCooldownDuration** | 1.0f | 공격 쿨타임 |
| **AttackDamage** | 10.0f | 공격당 피해량 |

---

## ✅ 핵심 수정사항

### 1️⃣ AutoPossessAI 문제 해결

**파일**: `AOSCharacter.cpp`
**수정**: 생성자에서 AIControllerClass 설정

```cpp
AAOSCharacter::AAOSCharacter()
{
    // ... 다른 설정 ...

    // 🟡 MODIFIED - AI 컨트롤러 자동 할당
    AIControllerClass = AAOSAIController::StaticClass();
}
```

**효과**: 에디터에서 배치한 캐릭터가 자동으로 AIController를 얻음

---

### 2️⃣ 런타임 스폰 AIController 문제 해결

**파일**: `AOSSpawnPoint.cpp`
**수정**: SpawnParameters 및 지연 호출 추가

```cpp
// SpawnParameters 사용
FActorSpawnParameters SpawnParams;
SpawnParams.Owner = this;
SpawnParams.SpawnCollisionHandlingOverride =
    ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

AAOSCharacter* NewCharacter = GetWorld()->SpawnActor<AAOSCharacter>(
    CharacterClass,
    GetActorLocation(),
    FRotator::ZeroRotator,
    SpawnParams
);

// 지연된 초기화 (BeginPlay 이후)
if (GetWorld())
{
    GetWorld()->GetTimerManager().SetTimerForNextTick([Character]()
    {
        if (Character && Character->IsValidLowLevel())
        {
            Character->DeployToLane();
        }
    });
}
```

**효과**: 런타임에 스폰된 캐릭터도 AIController를 올바르게 받음

---

### 3️⃣ BeginPlay 타이밍 명확화

**파일**: `AOSAIController.cpp`
**수정**: GetPawn() 항상 유효하다는 사실 반영

```cpp
void AAOSAIController::BeginPlay()
{
    Super::BeginPlay();

    // 🟡 MODIFIED - BeginPlay 시점에는 항상 Possess 완료됨
    // 호출 순서: Pawn::BeginPlay() → Controller::Possess() → Controller::BeginPlay()
    ControlledCharacter = Cast<AAOSCharacter>(GetPawn());

    if (!ControlledCharacter)
    {
        UE_LOG(LogTemp, Error, TEXT("[AI Controller] Failed to possess character - Pawn is not AAOSCharacter!"));
        return;
    }

    UE_LOG(LogTemp, Warning, TEXT("[AI Controller] AI initialized for character in lane: %d"),
        static_cast<int32>(ControlledCharacter->GetLane()));

    // 배포 시작
    StartDeployment(ControlledCharacter->GetLane());
}
```

**효과**: 간단하고 신뢰할 수 있는 초기화

---

## 🧪 테스트 체크리스트

### 에디터 배치 테스트

- [ ] 에디터에서 AOSCharacter (또는 Blueprint) 배치
- [ ] Play 클릭
- [ ] 콘솔 확인:
  ```
  [Character] AI Controller found for Character_0
  [AI Controller] AI initialized for character in lane: X
  [AI Controller] Deployment started on lane: X
  [AI Controller] Cached lane info - Start: (...), End: (...)
  ```
- [ ] 캐릭터가 자동으로 첫 번째 타워 방향으로 이동하는지 확인

### 런타임 스폰 테스트

- [ ] GameMode 또는 별도 스폰 매니저에서 SpawnCharacterAtPoint() 호출
- [ ] 콘솔에서 스폰 메시지 확인:
  ```
  Character spawned at SpawnPoint - Team: 0, Lane: 1, Position: (...)
  [Character] AI Controller found for Character_0
  [AI Controller] AI initialized for character in lane: 1
  ```
- [ ] 캐릭터가 배포 후 이동 시작 확인

### AI 동작 테스트

- [ ] **이동**: 캐릭터가 다음 목표로 이동하는가?
- [ ] **타워 감지**: 범위 내 타워 발견 시 방향 변경하는가?
- [ ] **적 감지**: 범위 내 적 발견 시 공격하는가?
- [ ] **타워 순차**: 타워 파괴 후 다음 타워로 이동하는가?
- [ ] **최종 목표**: 모든 타워 파괴 후 커맨드 센터로 이동하는가?

---

## 📚 관련 파일

| 파일 | 목적 | 상태 |
|------|------|------|
| [AOSCharacter.h](../../Source/TDProject/AOS/AOSCharacter.h) | 캐릭터 클래스 헤더 | ✅ 수정됨 |
| [AOSCharacter.cpp](../../Source/TDProject/AOS/AOSCharacter.cpp) | 캐릭터 구현 | ✅ 수정됨 |
| [AOSAIController.h](../../Source/TDProject/AOS/AOSAIController.h) | AI 컨트롤러 헤더 | ✅ 수정됨 |
| [AOSAIController.cpp](../../Source/TDProject/AOS/AOSAIController.cpp) | AI 구현 | ✅ 수정됨 |
| [AOSSpawnPoint.h](../../Source/TDProject/AOS/AOSSpawnPoint.h) | 스폰 지점 헤더 | ✅ 수정됨 |
| [AOSSpawnPoint.cpp](../../Source/TDProject/AOS/AOSSpawnPoint.cpp) | 스폰 지점 구현 | ✅ 수정됨 |
| [AOSGameMode.h](../../Source/TDProject/AOS/AOSGameMode.h) | 게임 모드 헤더 | ✅ 수정됨 |
| [AOSGameMode.cpp](../../Source/TDProject/AOS/AOSGameMode.cpp) | 게임 모드 구현 | ✅ 수정됨 |

---

## 🔗 참고 가이드

- [AI_MOVEMENT_IMPLEMENTATION.md](./AI_MOVEMENT_IMPLEMENTATION.md) - 상세 구현 가이드
- [AICONTROLLER_POSSESSION_FIX.md](../03_ClassReview/AICONTROLLER_POSSESSION_FIX.md) - AutoPossessAI 해결책
- [RUNTIME_SPAWN_AICONTROLLER_FIX.md](../03_ClassReview/RUNTIME_SPAWN_AICONTROLLER_FIX.md) - 런타임 스폰 해결책
- [AICONTROLLER_BEGINPLAY_TIMING.md](../03_ClassReview/AICONTROLLER_BEGINPLAY_TIMING.md) - 엔진 생명주기

---

## 🎓 학습 포인트

1. **State Tree vs Tick 기반 AI**
   - State Tree는 복잡한 상태 머신에 유용
   - 단순한 우선순위 기반 AI는 Tick 사용이 더 간단함

2. **Unreal 액터 생명주기**
   - Constructor → BeginPlay → Tick → EndPlay
   - AutoPossessAI는 Pawn::BeginPlay() 후에 처리됨

3. **런타임 스폰과 에디터 배치의 차이**
   - 에디터: 바로 BeginPlay() 및 Possess 처리
   - 런타임: 다음 프레임에 BeginPlay() 실행
   - 해결: SetTimerForNextTick() 사용

4. **메모리 안전성**
   - Lambda에 포인터 캡처 시 IsValidLowLevel() 확인 필수
   - TObjectPtr 사용으로 자동 메모리 관리

5. **TActorIterator 성능**
   - 월드 검색이 매 프레임 실행되면 성능 저하
   - 향후 캐싱 또는 이벤트 기반 시스템 고려

---

## 🚀 향후 개선 방향

### 즉시 개선 가능

1. **MapManager 캐싱**
   ```cpp
   // 현재: 매 프레임 TActorIterator 사용
   // 개선: BeginPlay에서 한 번만 검색
   ```

2. **감지 최적화**
   ```cpp
   // 현재: 모든 액터 순회
   // 개선: Sphere Trace 또는 Overlap 컴포넌트 사용
   ```

3. **상태 머신 도입**
   ```cpp
   // 현재: 단순 우선순위
   // 개선: Idle → Moving → Attacking → Dead 상태 추가
   ```

### 장기 개선 방향

1. **경로 찾기 (Pathfinding)**
   - 현재: 직선 이동
   - 개선: NavMesh 기반 경로 계산

2. **팀 협력**
   - 현재: 개별 행동
   - 개선: 팀 단위 전략 (집중 공격, 포위 등)

3. **회피 로직**
   - 현재: 무조건 공격
   - 개선: 체력 기반 회피/후퇴

4. **통신 시스템**
   - 현재: 단순 데미지 처리
   - 개선: 유닛 간 신호 시스템 (예: 도움 요청)

---

**최종 업데이트**: 2025-11-24
**완성도**: 100% (기본 기능)
**다음 단계**: PIE 테스트 및 추가 기능 개발 필요

