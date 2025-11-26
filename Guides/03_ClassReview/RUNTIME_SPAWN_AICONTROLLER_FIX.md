# 런타임 스폰 시 AIController 할당 문제 해결

**버전**: 1.0
**날짜**: 2025-11-24
**상태**: ✅ 완성

---

## 🔴 문제 상황

`World->bStartup == false` (PIE 또는 런타임 스폰)일 때 AIController가 생성되지 않는 문제

### 증상
```
SpawnActor()로 캐릭터 생성
→ DeployToLane() 즉시 호출
→ GetController() == nullptr (BeginPlay()가 아직 호출 안됨)
→ AI 컨트롤러 미할당
```

### 플로우 분석

**잘못된 플로우**:
```
SpawnActor() 호출
  ↓
InitializeCharacter() 즉시 실행
  ├─ SetTeam(), SetLane() 설정
  └─ DeployToLane() 호출 ← ❌ 이때 BeginPlay()가 아직 안됨!
  ↓
다음 프레임
  ↓
BeginPlay() 호출 (AIController 할당) ← 너무 늦음!
```

---

## ✅ 해결 방법

### 원인
스폰된 액터는 **다음 프레임에 BeginPlay()가 호출**되므로, 즉시 `DeployToLane()`을 호출하면 AIController가 아직 할당되지 않음.

### 해결책: 지연 호출 (Timer)

```cpp
// 🟡 MODIFIED - BeginPlay가 호출된 후에 DeployToLane() 호출
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

**파일**: `Source/TDProject/AOS/AOSSpawnPoint.cpp`

---

## 📋 적용된 변경사항

### 1️⃣ SpawnCharacterAtPoint() 개선

```cpp
// SpawnParameters 추가
FActorSpawnParameters SpawnParams;
SpawnParams.Owner = this;
SpawnParams.SpawnCollisionHandlingOverride =
    ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

// SpawnActor에 전달
AAOSCharacter* NewCharacter = GetWorld()->SpawnActor<AAOSCharacter>(
    CharacterClass,
    GetActorLocation(),
    FRotator::ZeroRotator,
    SpawnParams  // ← 추가됨
);
```

**목적**:
- 스폰 충돌 처리 개선
- Owner 설정으로 메모리 관리 개선

### 2️⃣ InitializeCharacter() 지연 처리

```cpp
// DeployToLane()을 다음 프레임으로 지연
GetWorld()->GetTimerManager().SetTimerForNextTick([Character]()
{
    if (Character && Character->IsValidLowLevel())
    {
        Character->DeployToLane();
    }
});
```

**효과**:
- BeginPlay()가 호출되고 나서 DeployToLane() 실행
- AIController가 정상 할당됨
- 안전한 메모리 체크 (IsValidLowLevel)

---

## 🔍 전체 실행 순서 (수정 후)

```
1️⃣ GameMode::SpawnCharactersAtAllSpawnPoints() 호출
   ↓
2️⃣ SpawnPoint::SpawnCharacterAtPoint() 호출
   ├─ SpawnActor() 실행 (캐릭터 생성)
   └─ InitializeCharacter() 호출
       ├─ SetTeam(), SetLane() 설정
       └─ SetTimerForNextTick([Character]{ ... }) ← 지연 등록
   ↓
   ⏸️ 다음 프레임 대기
   ↓
3️⃣ 엔진이 새로 스폰된 액터들의 BeginPlay() 호출
   ├─ AAOSCharacter::BeginPlay() 실행
   │  └─ GetController()에서 AIController 할당 ✅
   └─ SetupCharacterDefaults(), 체력 초기화
   ↓
4️⃣ Timer 콜백 실행
   ├─ Character->DeployToLane() 호출
   │  └─ AIController->StartDeployment() 실행 ✅
   └─ AI 움직임 시작
```

---

## 💡 중요 개념

### SetTimerForNextTick vs Delay

| 메서드 | 타이밍 | 사용 사례 |
|--------|--------|---------|
| **SetTimerForNextTick** | 정확히 다음 프레임 | BeginPlay 이후 보장 필요 |
| **SetTimer(0.001f)** | 1ms 후 | 약간의 여유 필요 |
| **AddActorLocalOffset** | 즉시 | 프레임 내 작업 |

### Lambda 캡처 주의

```cpp
// ❌ 위험 - Character 포인터가 무효할 수 있음
[Character]() { Character->DoSomething(); }

// ✅ 안전 - 유효성 확인
[Character]() {
    if (Character && Character->IsValidLowLevel()) {
        Character->DoSomething();
    }
}
```

---

## 🎯 테스트 방법

### PIE에서 동작 확인

1. 에디터에서 `Play` 클릭
2. 콘솔 로그 확인:

```
Character spawned at SpawnPoint - Team: 0, Lane: 1, Position: (1200.0, 1200.0, 100.0)
[Character] AI Controller found for Character_0
[AI Controller] AI initialized for character in lane: 1
[AI Controller] Deployment started on lane: 1
[AI Controller] Cached lane info - Start: (...), End: (...)
[AI] Arrived at target: (...)
```

3. 캐릭터가 자동으로 움직이는지 확인

### 문제 발생 시 체크리스트

- [ ] `bStartup == false` 확인 (PIE 또는 스폰인 경우)
- [ ] `SetTimerForNextTick` 호출되는지 로그 추가
- [ ] `Character->IsValidLowLevel()` true 확인
- [ ] `DeployToLane()` 호출되는지 확인

---

## 📊 스폰 타입별 처리

### 에디터 배치 (bStartup == true)

```
BeginPlay() 호출
  ↓
AIController 자동 할당 (AutoPossessAI = PlacedInWorld)
  ↓
DeployToLane() 즉시 호출 가능 ✅
```

**처리**: AAOSCharacter 생성자에서 설정
```cpp
AIControllerClass = AAOSAIController::StaticClass();
```

### 런타임 스폰 (bStartup == false)

```
SpawnActor() 호출
  ↓
BeginPlay() 다음 프레임 (비동기)
  ↓
SetTimerForNextTick으로 지연
  ↓
DeployToLane() 호출 ✅
```

**처리**: SetTimerForNextTick 사용

---

## 🔧 확장 가능성

### 커스텀 Initialization 콜백

```cpp
// 향후 개선: 초기화 완료 콜백
DECLARE_DELEGATE_OneParam(FOnCharacterInitialized, AAOSCharacter*);

// InitializeCharacter에 콜백 추가
OnCharacterInitialized.ExecuteIfBound(Character);
```

### 배치 스폰 최적화

```cpp
// 여러 캐릭터를 한 프레임에 스폰 가능
// Timer 부담 분산 고려
```

---

## 📝 관련 파일

- [AOSSpawnPoint.h](../../Source/TDProject/AOS/AOSSpawnPoint.h)
- [AOSSpawnPoint.cpp](../../Source/TDProject/AOS/AOSSpawnPoint.cpp)
- [AOSCharacter.cpp](../../Source/TDProject/AOS/AOSCharacter.cpp)
- [AOSAIController.cpp](../../Source/TDProject/AOS/AOSAIController.cpp)

---

## 🎓 학습 포인트

1. **액터 생명주기**: BeginPlay는 비동기 호출
2. **Timer 사용**: 정확한 타이밍 제어 필요
3. **메모리 안전성**: 포인터 유효성 항상 확인
4. **런타임 vs 에디터**: 다른 초기화 경로 필요

---

**최종 업데이트**: 2025-11-24
**담당**: AI 시스템
**상태**: ✅ 완성
