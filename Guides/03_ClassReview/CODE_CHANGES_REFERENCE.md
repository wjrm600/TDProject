# 코드 변경 사항 참고서 (빠른 검색용)

**목적**: 각 파일의 정확한 변경 위치와 내용을 빠르게 찾기

---

## 📂 파일별 변경사항 위치

### AOSSpawnPoint.h

| 변경 타입 | 위치 | 내용 |
|----------|------|------|
| 🟢 NEW | Line 53-59 | 두 함수 추가: `SpawnCharacterAtPoint()`, `InitializeCharacter()` |

**변경 전**:
```cpp
void SetOccupiedCharacter(class AAOSCharacter* Character);

UFUNCTION(BlueprintCallable, Category = "AOS|Spawn")
void ReleaseCharacter();

protected:
```

**변경 후**:
```cpp
void SetOccupiedCharacter(class AAOSCharacter* Character);

UFUNCTION(BlueprintCallable, Category = "AOS|Spawn")
void ReleaseCharacter();

// 🟢 NEW - 스폰 포인트에서 캐릭터 생성
UFUNCTION(BlueprintCallable, Category = "AOS|Spawn")
class AAOSCharacter* SpawnCharacterAtPoint(TSubclassOf<class AAOSCharacter> CharacterClass);

// 🟢 NEW - 생성된 캐릭터 초기화
UFUNCTION(BlueprintCallable, Category = "AOS|Spawn")
void InitializeCharacter(class AAOSCharacter* Character);

protected:
```

---

### AOSSpawnPoint.cpp

| 변경 타입 | 위치 | 내용 |
|----------|------|------|
| 🟢 NEW | Line 47-68 | `SpawnCharacterAtPoint()` 구현 |
| 🟢 NEW | Line 71-91 | `InitializeCharacter()` 구현 |

**추가 코드**:
```cpp
// 🟢 NEW - 스폰 포인트에서 캐릭터 생성
AAOSCharacter* AAOSSpawnPoint::SpawnCharacterAtPoint(TSubclassOf<AAOSCharacter> CharacterClass)
{
	if (!CharacterClass || !GetWorld())
	{
		return nullptr;
	}

	// 스폰 포인트 위치에 캐릭터 생성
	AAOSCharacter* NewCharacter = GetWorld()->SpawnActor<AAOSCharacter>(
		CharacterClass,
		GetActorLocation(),
		FRotator::ZeroRotator
	);

	if (NewCharacter)
	{
		InitializeCharacter(NewCharacter);
	}

	return NewCharacter;
}

// 🟢 NEW - 생성된 캐릭터 초기화
void AAOSSpawnPoint::InitializeCharacter(AAOSCharacter* Character)
{
	if (!Character)
	{
		return;
	}

	// 캐릭터 설정
	Character->SetTeam(Team);
	Character->SetLane(Lane);
	Character->SetActorLocation(GetActorLocation());

	// 스폰 포인트에 등록
	SetOccupiedCharacter(Character);

	// AI 시작
	Character->DeployToLane();

	UE_LOG(LogTemp, Warning, TEXT("Character spawned at SpawnPoint - Team: %d, Lane: %d"),
	       static_cast<int32>(Team), static_cast<int32>(Lane));
}
```

---

### AOSGameMode.h

| 변경 타입 | 위치 | 내용 |
|----------|------|------|
| 🔴 REMOVED | Line 67-68 | `SpawnCharacter()` 제거 표기 |
| 🟢 NEW | Line 70-72 | `SpawnCharactersAtAllSpawnPoints()` 추가 |

**변경 전**:
```cpp
UFUNCTION(BlueprintCallable, Category = "AOS|Spawn")
void SpawnCharacter(AAOSCharacter* Character, EAOSTeam Team, EAOSLane Lane);

// 구조물 생성 및 관리
```

**변경 후**:
```cpp
// 🔴 REMOVED: void SpawnCharacter(AAOSCharacter* Character, EAOSTeam Team, EAOSLane Lane);
// ↑ 더 이상 필요 없음 (자동 생성으로 변경)

// 🟢 NEW - 모든 스폰 포인트에서 캐릭터 자동 생성
UFUNCTION(BlueprintCallable, Category = "AOS|Spawn")
void SpawnCharactersAtAllSpawnPoints();

// 구조물 생성 및 관리
```

---

### AOSGameMode.cpp

#### 변경 1️⃣: StartGame() 수정

| 변경 타입 | 위치 | 내용 |
|----------|------|------|
| 🟡 MODIFIED | Line 46-57 | `SpawnCharactersAtAllSpawnPoints()` 호출 추가 |

**변경 전**:
```cpp
void AAOSGameMode::StartGame()
{
	if (AOSGameState == EAOSGameState::Preparation)
	{
		AOSGameState = EAOSGameState::GameRunning;
		RemainingGameTime = GameDuration;
	}
}
```

**변경 후**:
```cpp
// 🟡 MODIFIED - 캐릭터 자동 생성 로직 추가
void AAOSGameMode::StartGame()
{
	if (AOSGameState == EAOSGameState::Preparation)
	{
		AOSGameState = EAOSGameState::GameRunning;
		RemainingGameTime = GameDuration;

		// 🟢 NEW - 모든 스폰 포인트에서 캐릭터 자동 생성
		SpawnCharactersAtAllSpawnPoints();
	}
}
```

#### 변경 2️⃣: SpawnCharacter() 제거

| 변경 타입 | 위치 | 내용 |
|----------|------|------|
| 🔴 REMOVED | Line 124-131 | `SpawnCharacter()` 함수 주석 처리 |

**제거된 코드 (주석 처리)**:
```cpp
// 🔴 REMOVED: SpawnCharacter() 함수는 더 이상 사용되지 않습니다.
// SpawnCharactersAtAllSpawnPoints()로 대체되었습니다.
/*
void AAOSGameMode::SpawnCharacter(AAOSCharacter* Character, EAOSTeam Team, EAOSLane Lane)
{
	// ... 기존 구현 제거됨
}
*/
```

#### 변경 3️⃣: SpawnCharactersAtAllSpawnPoints() 구현

| 변경 타입 | 위치 | 내용 |
|----------|------|------|
| 🟢 NEW | Line 133-171 | 새 함수 구현 |

**추가 코드**:
```cpp
// 🟢 NEW - 모든 스폰 포인트에서 캐릭터 자동 생성
void AAOSGameMode::SpawnCharactersAtAllSpawnPoints()
{
	if (!DefaultPawnClass)
	{
		UE_LOG(LogTemp, Error, TEXT("DefaultPawnClass not set!"));
		return;
	}

	// DefaultPawnClass가 AAOSCharacter 파생 클래스인지 확인
	if (!DefaultPawnClass->IsChildOf(AAOSCharacter::StaticClass()))
	{
		UE_LOG(LogTemp, Error, TEXT("DefaultPawnClass is not a valid AAOSCharacter class!"));
		return;
	}

	for (AAOSSpawnPoint* SpawnPoint : AllSpawnPoints)
	{
		if (SpawnPoint)
		{
			// 스폰 포인트에서 캐릭터 생성
			AAOSCharacter* NewCharacter = SpawnPoint->SpawnCharacterAtPoint(TSubclassOf<AAOSCharacter>(DefaultPawnClass));

			if (NewCharacter)
			{
				// 팀별 리스트에 추가
				if (NewCharacter->GetTeam() == EAOSTeam::Team1)
				{
					Team1Characters.Add(NewCharacter);
				}
				else
				{
					Team2Characters.Add(NewCharacter);
				}
			}
		}
	}
}
```

---

### AOSPlayerController.cpp

| 변경 타입 | 위치 | 내용 |
|----------|------|------|
| 🔴 REMOVED | Line 87-95 | `SpawnPlayerCharacters()` 함수 제거 |

**제거된 코드 (주석 처리)**:
```cpp
// 🔴 REMOVED: SpawnPlayerCharacters() 함수는 더 이상 사용되지 않습니다.
// AOSGameMode의 SpawnCharactersAtAllSpawnPoints()로 대체되었습니다.
// 모든 캐릭터는 게임 시작 시 스폰 포인트에서 자동으로 생성됩니다.
/*
void AAOSPlayerController::SpawnPlayerCharacters()
{
	// ... 기존 구현 제거됨
}
*/
```

---

## 🔍 빠른 검색 가이드

### "스폰 포인트에서 캐릭터를 어떻게 생성하는가?"
→ [AOSSpawnPoint.cpp](Source/TDProject/AOS/AOSSpawnPoint.cpp) Line 47-68

### "게임 시작 시 어디서 캐릭터를 생성하는가?"
→ [AOSGameMode.cpp](Source/TDProject/AOS/AOSGameMode.cpp) Line 46-57

### "모든 스폰 포인트에서 캐릭터를 생성하는 함수는?"
→ [AOSGameMode.cpp](Source/TDProject/AOS/AOSGameMode.cpp) Line 133-171

### "생성된 캐릭터는 어떻게 초기화되는가?"
→ [AOSSpawnPoint.cpp](Source/TDProject/AOS/AOSSpawnPoint.cpp) Line 71-91

### "제거된 함수들은 무엇인가?"
→ 검색: 🔴 REMOVED
- [AOSGameMode.h](Source/TDProject/AOS/AOSGameMode.h) Line 67-68
- [AOSGameMode.cpp](Source/TDProject/AOS/AOSGameMode.cpp) Line 124-131
- [AOSPlayerController.cpp](Source/TDProject/AOS/AOSPlayerController.cpp) Line 87-95

---

## 📊 요약

| 파일 | 변경 타입 | 개수 | 라인 |
|------|----------|------|------|
| AOSSpawnPoint.h | 🟢 NEW | 2함수 | 53-59 |
| AOSSpawnPoint.cpp | 🟢 NEW | 2함수 | 47-91 |
| AOSGameMode.h | 🟢 NEW + 🔴 REMOVED | 2 | 67-72 |
| AOSGameMode.cpp | 🟡 MODIFIED + 🟢 NEW + 🔴 REMOVED | 3 | 46-171 |
| AOSPlayerController.cpp | 🔴 REMOVED | 1함수 | 87-95 |

**총 변경**: 11개 (NEW 6, MODIFIED 1, REMOVED 4)

---

## 🧪 테스트 체크리스트

### 구현 검증

- [x] AOSSpawnPoint.h에 2개 함수 선언 추가
- [x] AOSSpawnPoint.cpp에 2개 함수 구현
- [x] AOSGameMode.h에 새 함수 선언 추가
- [x] AOSGameMode.h에 제거 표기 추가
- [x] AOSGameMode.cpp의 StartGame() 수정
- [x] AOSGameMode.cpp의 SpawnCharacter() 제거
- [x] AOSGameMode.cpp의 SpawnCharactersAtAllSpawnPoints() 구현
- [x] AOSPlayerController.cpp의 SpawnPlayerCharacters() 제거

### 컴파일 검증

- [x] 프로젝트 컴파일 성공
- [x] 모든 에러 0개
- [x] 모든 경고 확인 (Visual Studio 버전 경고는 무시)

### 다음 테스트

- [ ] BP_AOSCharacter 블루프린트 생성
- [ ] 레벨에 12개 스폰 포인트 배치
- [ ] DefaultPawnClass 설정
- [ ] PIE에서 캐릭터 자동 생성 확인

---

**마지막 업데이트**: 2025-11-17
**컴파일 상태**: ✅ 성공
