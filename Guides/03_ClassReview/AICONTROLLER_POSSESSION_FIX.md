# AutoPossessAI 문제 해결 가이드

**버전**: 1.0
**날짜**: 2025-11-24
**상태**: ✅ 완성

---

## 🔴 문제 상황

`AutoPossessAI = PlacedInWorld` 설정이 있어도 AIController가 생성되지 않는 문제

### 증상
```
BeginPlay()에서 GetController()가 nullptr 반환
AI 움직임이 작동하지 않음
콘솔: "[Character] AI Controller NOT found for..."
```

---

## ✅ 해결 방법

### 핵심 원인

Unreal Engine의 Pawn 클래스가 `AutoPossessAI`를 자동으로 처리하려면, 다음이 필요합니다:

1. **Pawn.h에서 설정된 `AIControllerClass`**
2. **GameMode에 등록된 Controller 클래스**

### 해결책 - AAOSCharacter 생성자에서 설정

```cpp
AAOSCharacter::AAOSCharacter()
{
    // ... 다른 설정들 ...

    // 🟡 MODIFIED - AI 컨트롤러 자동 할당
    // AutoPossessAI = PlacedInWorld일 때 작동하려면 이렇게 설정해야 함
    AIControllerClass = AAOSAIController::StaticClass();
}
```

**파일**: `Source/TDProject/AOS/AOSCharacter.cpp`

---

## 📋 적용 단계

### Step 1: Pawn에서 AIControllerClass 설정

```cpp
// AOSCharacter::AOSCharacter() 생성자
AIControllerClass = AAOSAIController::StaticClass();
```

### Step 2: BeginPlay()에서 Controller 확인

```cpp
void AAOSCharacter::BeginPlay()
{
    Super::BeginPlay();

    // AI 컨트롤러 설정
    AAOSAIController* AIController = Cast<AAOSAIController>(GetController());
    if (AIController)
    {
        AOSAIController = AIController;
        UE_LOG(LogTemp, Warning, TEXT("[Character] AI Controller found for %s"), *GetName());
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("[Character] AI Controller NOT found for %s"), *GetName());
    }
}
```

### Step 3: Blueprint에서 설정 확인

Blueprint 에디터에서:
1. `Defaults` 탭 열기
2. `Pawn` 섹션 찾기
3. `AutoPossessAI` = **PlacedInWorld** 선택
4. `AIControllerClass` = **/Script/TDProject.AOSAIController_C** (자동으로 설정됨)

---

## 🔍 디버깅 팁

### 콘솔 출력으로 확인

```
[Character] AI Controller found for CharacterName_0
  → ✅ 성공 - AI가 정상 작동

[Character] AI Controller NOT found for CharacterName_0
  → ❌ 실패 - AIControllerClass 확인 필요
```

### 체크리스트

- [ ] AOSCharacter 생성자에서 `AIControllerClass` 설정
- [ ] Blueprint가 AAOSCharacter를 상속
- [ ] Blueprint `AutoPossessAI` = **PlacedInWorld**
- [ ] GameMode가 AOSGameMode 사용
- [ ] PIE 또는 게임 실행에서 확인

---

## 📊 자동 Possession 플로우

```
게임 시작
  ↓
World에서 Pawn 스폰
  ↓
Pawn::BeginPlay() 호출
  ↓
AutoPossessAI 체크
  ├─ PlacedInWorld 설정됨?
  │  ↓
  │  AIControllerClass 확인
  │  ├─ 설정됨 → AIController 생성 및 할당 ✅
  │  └─ 미설정 → 할당 안함 ❌
  │
  └─ Disabled → 아무것도 안함
  ↓
Pawn::BeginPlay() 계속 진행
  ↓
GetController() 호출 가능 ✅
```

---

## 🎯 전체 작동 순서 (최종)

1. **AAOSCharacter 스폰**
   - 생성자에서 `AIControllerClass = AAOSAIController::StaticClass()`

2. **BeginPlay() 호출**
   - Super::BeginPlay() 실행 → AutoPossessAI 자동 처리
   - AIController가 자동으로 이 Pawn을 Possess

3. **AI 초기화**
   ```cpp
   AAOSAIController* AIController = Cast<AAOSAIController>(GetController());
   AOSAIController = AIController;  // 캐시
   ```

4. **AI 배포**
   ```cpp
   DeployToLane();
     ↓
   AOSAIController->StartDeployment(AssignedLane);
   ```

5. **Tick에서 AI 작동**
   ```cpp
   UpdateAIBehavior(DeltaTime)
     ├─ 적군 감지
     ├─ 목표 계산
     └─ 움직임 실행
   ```

---

## 💡 관련 개념

### AutoPossessAI 옵션

| 옵션 | 설명 |
|------|------|
| **Disabled** | AIController 할당 없음 (플레이어가 수동 제어) |
| **PlacedInWorld** | 월드에 배치된 Pawn을 AIController가 자동 할당 |
| **SpawnedByDefault** | 런타임에 스폰된 Pawn을 자동 할당 |
| **SpawnedByPlayer** | 플레이어가 스폰한 Pawn을 자동 할당 |

### AIControllerClass 설정 위치

| 방법 | 적용 시점 | 장점 |
|------|---------|------|
| **Pawn 생성자** | 컴파일 타임 | 안정적, 모든 인스턴스에 적용 |
| **Blueprint** | 에디터 | 유연함, 인스턴스별 다름 |
| **GameMode** | 런타임 | 게임 규칙 기반 |

---

## 📝 코드 참고

### 관련 파일
- [AOSCharacter.h](../../Source/TDProject/AOS/AOSCharacter.h)
- [AOSCharacter.cpp](../../Source/TDProject/AOS/AOSCharacter.cpp)
- [AOSAIController.h](../../Source/TDProject/AOS/AOSAIController.h)
- [AOSAIController.cpp](../../Source/TDProject/AOS/AOSAIController.cpp)

### 핵심 코드

**AOSCharacter.cpp (생성자)**
```cpp
AAOSCharacter::AAOSCharacter()
{
    // ... 다른 설정 ...
    AIControllerClass = AAOSAIController::StaticClass();
}
```

**AOSCharacter.cpp (BeginPlay)**
```cpp
void AAOSCharacter::BeginPlay()
{
    Super::BeginPlay();

    AAOSAIController* AIController = Cast<AAOSAIController>(GetController());
    if (AIController)
    {
        AOSAIController = AIController;
    }
}
```

---

## 🧪 테스트 방법

### PIE 테스트

1. `Play` 버튼 클릭
2. 콘솔에서 다음 메시지 확인:
   ```
   [Character] AI Controller found for Character_0
   [AI Controller] AI initialized for character in lane: 1
   [AI Controller] Deployment started on lane: 1
   ```
3. 캐릭터가 목표 타워로 이동하는지 확인

### 실패 케이스

```
[Character] AI Controller NOT found for Character_0
```

**원인**:
- [ ] `AIControllerClass` 미설정
- [ ] Blueprint에서 `AutoPossessAI` 미설정
- [ ] Pawn 클래스가 AAOSCharacter 상속 안함

---

**최종 업데이트**: 2025-11-24
**담당**: AI 시스템
**상태**: ✅ 완성
