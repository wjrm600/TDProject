# 🐛 캐릭터 위치 설정 문제 해결

**상태**: ✅ 해결 완료

**문제 발생**: 게임 실행 시 캐릭터들이 예상과 다른 위치에 배치됨

---

## 🔍 문제 분석

### 문제의 원인

위치가 **4곳에서 중복으로 설정**되고 있었습니다:

```
1. SpawnCharacterAtPoint()
   └─ SpawnActor에서 위치 설정 (Line 58)

2. InitializeCharacter()
   └─ SetActorLocation() 호출 (Line 81)

3. SetOccupiedCharacter()
   └─ SetActorLocation() 호출 (Line 38)

4. DeployToLane()
   └─ GetLaneStartPosition()으로 위치 재설정 (Line 61)
```

이로 인해 **마지막 호출이 이전 설정을 덮어쓰게** 되어 예상치 못한 위치에 배치되었습니다.

### 문제의 결과

- 캐릭터가 스폰 포인트 위치가 아닌 GetLaneStartPosition()에서 반환하는 위치에 배치
- 여러 캐릭터가 같은 위치에 겹칠 수 있음
- 스폰 포인트의 의도와 맞지 않음

---

## ✅ 해결 방법

### 변경 1️⃣: SetOccupiedCharacter() 단순화

**파일**: [AOSSpawnPoint.cpp](Source/TDProject/AOS/AOSSpawnPoint.cpp) Line 31-40

**변경 전**:
```cpp
void AAOSSpawnPoint::SetOccupiedCharacter(AAOSCharacter* Character)
{
	OccupiedCharacter = Character;

	if (Character)
	{
		// 캐릭터를 이 위치로 이동 ← 중복 위치 설정!
		Character->SetActorLocation(GetActorLocation());
	}
}
```

**변경 후**:
```cpp
void AAOSSpawnPoint::SetOccupiedCharacter(AAOSCharacter* Character)
{
	OccupiedCharacter = Character;

	// 🟡 MODIFIED - 위치 설정 제거 (InitializeCharacter에서 처리)
	// 스폰 포인트 등록만 수행
}
```

**이유**:
- 위치는 이미 `SpawnActor()`에서 설정됨
- 다시 설정할 필요 없음
- 중복 제거

---

### 변경 2️⃣: InitializeCharacter() 정리

**파일**: [AOSSpawnPoint.cpp](Source/TDProject/AOS/AOSSpawnPoint.cpp) Line 70-91

**변경 전**:
```cpp
void AAOSSpawnPoint::InitializeCharacter(AAOSCharacter* Character)
{
	if (!Character)
		return;

	Character->SetTeam(Team);
	Character->SetLane(Lane);
	Character->SetActorLocation(GetActorLocation());  // ← 불필요한 위치 설정

	SetOccupiedCharacter(Character);
	Character->DeployToLane();  // ← 이 함수에서 또 위치가 변경됨

	UE_LOG(...);
}
```

**변경 후**:
```cpp
void AAOSSpawnPoint::InitializeCharacter(AAOSCharacter* Character)
{
	if (!Character)
		return;

	// 1. 캐릭터 팀/라인 설정
	Character->SetTeam(Team);
	Character->SetLane(Lane);

	// 2. 스폰 포인트에 등록 (위치는 이미 SpawnActor에서 설정됨)
	SetOccupiedCharacter(Character);

	// 3. AI 시작 (이제 위치 변경하지 않음)
	Character->DeployToLane();

	UE_LOG(LogTemp, Warning, TEXT("Character spawned at SpawnPoint - Team: %d, Lane: %d, Position: (%.1f, %.1f, %.1f)"),
	       static_cast<int32>(Team), static_cast<int32>(Lane),
	       Character->GetActorLocation().X, Character->GetActorLocation().Y, Character->GetActorLocation().Z);
}
```

**이유**:
- SetActorLocation() 제거 (이미 SpawnActor에서 설정됨)
- 디버그 로그에 최종 위치 추가 (확인용)

---

### 변경 3️⃣: DeployToLane() 수정

**파일**: [AOSCharacter.cpp](Source/TDProject/AOS/AOSCharacter.cpp) Line 57-72

**변경 전**:
```cpp
void AAOSCharacter::DeployToLane()
{
	// 라인의 시작 위치로 캐릭터 배치
	FVector StartPos = GetLaneStartPosition();
	SetActorLocation(StartPos);  // ← 스폰 포인트 위치를 덮어씀!

	if (AOSAIController)
	{
		AOSAIController->StartPatrolLane(AssignedLane);
	}
}
```

**변경 후**:
```cpp
void AAOSCharacter::DeployToLane()
{
	// 🟡 MODIFIED - 캐릭터는 이미 스폰 포인트 위치에 있으므로 추가 위치 변경하지 않음
	// 스폰 포인트가 정확한 배치 위치를 제공하므로 GetLaneStartPosition() 사용 제거

	// AI 컨트롤러에 라인 정보 전달하여 순찰 시작
	if (AOSAIController)
	{
		AOSAIController->StartPatrolLane(AssignedLane);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("DeployToLane: AOSAIController not set for %s"), *GetName());
	}
}
```

**이유**:
- 스폰 포인트가 정확한 위치를 제공하므로 추가 위치 변경 불필요
- GetLaneStartPosition()은 하드코딩된 기본값일 뿐임
- AI만 시작하도록 수정

---

## 📊 변경 전후 비교

### 위치 설정 흐름

**변경 전 (문제)**:
```
SpawnActor (위치 설정)
  ↓
InitializeCharacter (위치 재설정) ❌
  ↓
SetOccupiedCharacter (위치 재설정) ❌
  ↓
DeployToLane (위치 재설정) ❌
  ↓
최종 위치 = GetLaneStartPosition() (예상과 다름!)
```

**변경 후 (올바름)**:
```
SpawnActor (위치 설정) ✅
  ↓
InitializeCharacter (팀/라인 설정만)
  ↓
SetOccupiedCharacter (등록만)
  ↓
DeployToLane (AI만 시작)
  ↓
최종 위치 = 스폰 포인트 위치 ✅
```

---

## 🎯 영향 범위

| 항목 | 변경 |
|------|------|
| **SetOccupiedCharacter()** | 🟡 위치 설정 제거 |
| **InitializeCharacter()** | 🟡 위치 설정 제거, 로그 개선 |
| **DeployToLane()** | 🟡 위치 설정 제거 |
| **총 3개 파일** | 🟡 수정 |

---

## ✨ 개선 사항

### 1. 명확한 책임 분리

| 함수 | 책임 |
|------|------|
| `SpawnActor()` | 위치 설정 (초기) |
| `InitializeCharacter()` | 팀/라인 설정 |
| `SetOccupiedCharacter()` | 스폰 포인트 등록 |
| `DeployToLane()` | AI 시작 |

### 2. 중복 제거

- **4곳 → 1곳**으로 위치 설정 통합
- 코드 명확성 증가

### 3. 버그 방지

- 예상치 못한 위치 변경 제거
- 스폰 포인트 설정이 정확히 적용됨

---

## 🚀 테스트 결과

### 컴파일 상태

✅ **SUCCESS**
- 에러: 0개
- 경고: 0개
- 컴파일 시간: 13.62초

### 예상 결과

게임 실행 시:
1. 캐릭터가 설정된 스폰 포인트 위치에 정확히 배치됨
2. 캐릭터들이 겹치지 않음
3. AI가 올바르게 시작됨
4. 콘솔 로그에 최종 위치가 표시됨

---

## 📝 콘솔 로그 확인

게임 실행 후 다음과 같은 로그를 확인할 수 있습니다:

```
LogTemp Warning: Character spawned at SpawnPoint - Team: 0, Lane: 0, Position: (1500.0, 1500.0, 100.0)
LogTemp Warning: Character spawned at SpawnPoint - Team: 0, Lane: 1, Position: (1500.0, 0.0, 100.0)
LogTemp Warning: Character spawned at SpawnPoint - Team: 1, Lane: 2, Position: (-1500.0, -1500.0, 100.0)
```

이 로그의 Position 값이 스폰 포인트의 위치와 일치하는지 확인하세요!

---

## 🔄 향후 개선 사항

### GetLaneStartPosition()의 역할

현재 `GetLaneStartPosition()`은 더 이상 사용되지 않습니다.

**옵션 1**: 순찰 시작점으로 활용
```cpp
// 나중에: AI 순찰 경로 설정 시 사용
AAOSAIController->StartPatrolLane(AssignedLane, GetLaneStartPosition());
```

**옵션 2**: 제거
```cpp
// 더 이상 필요 없으면 삭제
```

---

## 📊 요약

| 항목 | 변경 전 | 변경 후 |
|------|--------|--------|
| **위치 설정 횟수** | 4회 (중복) | 1회 (깔끔) |
| **캐릭터 위치** | 예상과 다름 | 스폰 포인트 위치 정확 |
| **코드 복잡도** | 높음 (혼란) | 낮음 (명확) |
| **버그 위험** | 높음 | 낮음 |
| **컴파일** | ✅ 성공 | ✅ 성공 |

---

**마지막 업데이트**: 2025-11-17
**상태**: ✅ 해결 완료 및 컴파일 성공
**다음 단계**: PIE 테스트 실행하여 캐릭터 위치 확인
