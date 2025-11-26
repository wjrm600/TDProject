# AIController::BeginPlay() GetPawn() nullptr 문제

**버전**: 1.0
**날짜**: 2025-11-24
**상태**: ✅ 완성

---

## 🟢 정확한 상황

AIController::BeginPlay()에서 `GetPawn()`은 **항상 유효**합니다!

```cpp
void AAOSAIController::BeginPlay()
{
    Super::BeginPlay();

    AAOSCharacter* Pawn = Cast<AAOSCharacter>(GetPawn());  // ✅ 항상 할당됨
    // ...
}
```

---

## 📊 호출 순서 분석

### Unreal Engine의 정확한 Possession 타이밍

```
1️⃣ AAOSSpawnPoint::SpawnCharacterAtPoint()
   └─ GetWorld()->SpawnActor<AAOSCharacter>()
      └─ 캐릭터 생성 및 BeginPlay() 예약

2️⃣ 엔진의 BeginPlay 배치 처리 (다음 프레임)
   ├─ Step A: Pawn::BeginPlay() 호출
   │  └─ AAOSCharacter::BeginPlay()
   │     └─ GetController() = nullptr (아직 할당 전)
   │
   ├─ Step B: Post-BeginPlay 처리
   │  └─ AutoPossessAI 활성화
   │     └─ AController::Possess(Pawn) 호출 ← ✅ 할당!
   │
   └─ Step C: Controller::BeginPlay() 호출
      └─ AAOSAIController::BeginPlay()
         └─ GetPawn() = 유효한 Pawn ✅
```

### 핵심: Controller BeginPlay는 Possess 이후!

**Pawn::BeginPlay** → **Possess** → **Controller::BeginPlay**

따라서 Controller::BeginPlay에서 GetPawn()은 **항상 유효**합니다!

---

## ✅ 해결책

### 방법: 안전한 지연 초기화

```cpp
void AAOSAIController::BeginPlay()
{
    Super::BeginPlay();

    // 1. 먼저 시도
    ControlledCharacter = Cast<AAOSCharacter>(GetPawn());

    if (!ControlledCharacter)
    {
        // 2. 실패하면 다음 틱까지 대기
        if (GetWorld())
        {
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
        }
        return;
    }

    // 3. 바로 할당되면 즉시 시작
    StartDeployment(ControlledCharacter->GetLane());
}
```

**장점**:
- ✅ 양쪽 경우 모두 처리 (즉시 할당, 지연 할당)
- ✅ nullptr 크래시 방지
- ✅ 안전한 메모리 체크 (IsValidLowLevel)
- ✅ 로깅으로 타이밍 파악 가능

---

## 📈 실행 플로우 (수정 후)

### Case 1: GetPawn() 즉시 성공

```
AIController::BeginPlay()
  ├─ GetPawn() ← 이미 할당됨 ✅
  ├─ ControlledCharacter 설정
  └─ StartDeployment() 즉시 호출
     └─ AI 배포 시작 ✅
```

**콘솔**:
```
[AI Controller] AI initialized for character in lane: 1
[AI Controller] Deployment started on lane: 1
```

### Case 2: GetPawn() 지연 성공

```
AIController::BeginPlay()
  ├─ GetPawn() ← nullptr ⚠️
  ├─ SetTimerForNextTick() 등록
  └─ return

⏸️ 다음 프레임

Timer 콜백 실행
  ├─ GetPawn() ← 이번엔 할당됨 ✅
  ├─ ControlledCharacter 설정
  └─ StartDeployment() 호출
     └─ AI 배포 시작 ✅
```

**콘솔**:
```
[AI Controller] Pawn not yet possessed (normal during startup). Will initialize when ready.
[AI Controller] AI initialized for character in lane: 1
[AI Controller] Deployment started on lane: 1
```

### Case 3: GetPawn() 계속 nullptr (에러)

```
AIController::BeginPlay()
  ├─ GetPawn() ← nullptr
  └─ SetTimerForNextTick() 등록

⏸️ 다음 프레임

Timer 콜백 실행
  ├─ GetPawn() ← 여전히 nullptr ❌
  └─ 에러 로그 출력
     └─ [AI Controller] Failed to possess character
```

**원인 확인**:
1. AAOSCharacter 생성자에서 AIControllerClass 설정?
2. Pawn이 실제로 AAOSCharacter인가?
3. World->bStartup 상태 확인

---

## 🔧 체크리스트

### 문제 진단

콘솔 로그 확인:

```
✅ 정상:
[AI Controller] AI initialized for character in lane: X
[AI Controller] Deployment started on lane: X

⚠️ 지연 정상:
[AI Controller] Pawn not yet possessed...
[AI Controller] AI initialized for character in lane: X

❌ 에러:
[AI Controller] Failed to possess character
```

### 해결 순서

1. **AAOSCharacter 생성자 확인**
   ```cpp
   AIControllerClass = AAOSAIController::StaticClass();
   ```

2. **Blueprint 설정 확인**
   - `AutoPossessAI = PlacedInWorld`

3. **GameMode 확인**
   - DefaultPawnClass = AAOSCharacter
   - 스폰 로직이 올바른가?

4. **런타임 스폰 확인**
   - SetTimerForNextTick 사용하는가?

---

## 📝 Possession 보장 방법

### 방법 1: 지연 초기화 (현재 방식) ✅ 추천

```cpp
// GetPawn() 실패 시 다음 틱 재시도
if (!GetPawn())
{
    SetTimerForNextTick([this]{ InitializeAI(); });
}
```

**장점**: 단순, 안전, 대부분 작동

### 방법 2: Possess 콜백

```cpp
virtual void OnPossess(APawn* InPawn) override
{
    Super::OnPossess(InPawn);
    // InPawn이 항상 유효함
}
```

**장점**: 정확, 콜백 기반
**단점**: 추가 구현 필요

### 방법 3: 강제 Possess

```cpp
void AAOSAIController::BeginPlay()
{
    Super::BeginPlay();

    APawn* Pawn = GetPawn();
    if (!Pawn)
    {
        Pawn = GetWorld()->SpawnActor<APawn>(...);
        Possess(Pawn);
    }
}
```

**장점**: 완전한 제어
**단점**: 복잡, 메모리 관리 필요

---

## 🎯 Best Practice

### 권장 구현

```cpp
// 1. Pawn 클래스: AIControllerClass 설정
AAOSCharacter::AAOSCharacter()
{
    AIControllerClass = AAOSAIController::StaticClass();
}

// 2. Controller BeginPlay: 안전한 체크
void AAOSAIController::BeginPlay()
{
    Super::BeginPlay();

    ControlledCharacter = Cast<AAOSCharacter>(GetPawn());
    if (!ControlledCharacter)
    {
        GetWorld()->GetTimerManager().SetTimerForNextTick([this]()
        {
            ControlledCharacter = Cast<AAOSCharacter>(GetPawn());
            if (ControlledCharacter)
            {
                StartDeployment(ControlledCharacter->GetLane());
            }
        });
        return;
    }

    StartDeployment(ControlledCharacter->GetLane());
}

// 3. Pawn BeginPlay: AIController 캐싱
void AAOSCharacter::BeginPlay()
{
    Super::BeginPlay();

    AAOSAIController* AIController = Cast<AAOSAIController>(GetController());
    if (AIController)
    {
        AOSAIController = AIController;
    }
}

// 4. 초기화: 지연 처리
void AAOSSpawnPoint::InitializeCharacter(AAOSCharacter* Character)
{
    // ...
    GetWorld()->GetTimerManager().SetTimerForNextTick([Character]()
    {
        if (Character && Character->IsValidLowLevel())
        {
            Character->DeployToLane();
        }
    });
}
```

### 로깅으로 타이밍 파악

```cpp
// BeginPlay 시작
UE_LOG(LogTemp, Warning, TEXT("[AI] BeginPlay called"));

// GetPawn 결과
if (!GetPawn())
{
    UE_LOG(LogTemp, Warning, TEXT("[AI] GetPawn() nullptr - will retry"));
}

// Timer 콜백
UE_LOG(LogTemp, Warning, TEXT("[AI] Timer callback - GetPawn() ready"));
```

---

## 📚 참고 자료

### Unreal Engine 생명주기

1. **SpawnActor** - 액터 생성
2. **PostInitializeComponents** - 컴포넌트 초기화
3. **BeginPlay** - 게임 로직 시작
4. **Tick** - 매 프레임 업데이트

### AIController 관련

- `AutoPossessAI` - Pawn 자동 할당 설정
- `Possess()` - Pawn 할당 함수
- `OnPossess()` - Possession 콜백
- `GetPawn()` - 제어 중인 Pawn 가져오기

---

**최종 업데이트**: 2025-11-24
**담당**: AI 시스템
**상태**: ✅ 완성
