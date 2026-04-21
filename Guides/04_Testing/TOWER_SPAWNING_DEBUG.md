# 🔍 타워 스포닝 디버깅 가이드

**Date**: 2025-11-23
**Issue**: "MapManager에 LanesInfo를 추가해도 아무것도 나오지 않음"
**Status**: ✅ 진단 완료 (디버그 로깅 추가)

---

## 📋 증상

MapManager의 LanesInfo에 Top/Mid/Bottom 3개를 수동으로 추가했는데:
- ❌ 타워가 화면에 보이지 않음
- ❌ 콘솔 로그도 없음
- ❌ 에러도 없음

---

## 🔧 진단 (Diagnostic)

### 코드 분석

**MapManager::InitializeMap()** 로직:
```cpp
void AAOSMapManager::InitializeMap()
{
    if (LanesInfo.Num() == 0)  // ← 여기가 문제!
    {
        SetupDefaultLaneInfo();  // 기본값 생성
    }
}
```

**문제점**:
1. 에디터에서 LanesInfo를 추가하면 `Num() > 0`
2. 따라서 `SetupDefaultLaneInfo()`가 호출되지 않음
3. 하지만 에디터에서 추가한 데이터가 **올바르지 않으면** 타워가 생성되지 않음

---

## 🐛 원인 분석

### 가능한 원인들:

#### 1️⃣ **LanesInfo 배열이 비어있음**
```
Expected: LanesInfo.Num() = 3
Actual: LanesInfo.Num() = 0 또는 잘못된 구조
```

#### 2️⃣ **LaneType이 올바르지 않음**
```cpp
// ❌ 잘못된 예
LaneType = EAOSLane::Max  // 정의되지 않은 값

// ✅ 올바른 예
LaneType = EAOSLane::Top
LaneType = EAOSLane::Mid
LaneType = EAOSLane::Bottom
```

#### 3️⃣ **타워 위치 배열이 비어있음**
```cpp
// ❌ 잘못된 예
Team1TowerPositions.Num() = 0

// ✅ 올바른 예
Team1TowerPositions = {
    FVector(1200, 1200, 0),
    FVector(600, 600, 0),
    FVector(0, 0, 0)
};
```

---

## ✅ 디버그 로깅 (새로 추가됨)

빌드 후 이제 콘솔에 다음과 같은 로그가 출력됩니다:

```
=== SpawnStructures Started ===
Total LanesInfo: 3

Processing Lane: 0
  Team1 Towers: 3
  Team2 Towers: 3
Team1 Tower spawned at (1200.0, 1200.0, 0.0)
Team1 Tower spawned at (600.0, 600.0, 0.0)
Team1 Tower spawned at (0.0, 0.0, 0.0)
[... 계속 ...]

=== Spawning Command Centers ===
Mid Lane found, spawning command centers
Team1 Command Center spawned at (-1500.0, 0.0, 0.0)
Team2 Command Center spawned at (1500.0, 0.0, 0.0)

=== SpawnStructures Complete ===
```

---

## 🧪 디버깅 단계

### 1단계: 콘솔 로그 확인

**게임 실행 후**:
```
Ctrl+` (백틱) → 콘솔 열기
```

**다음을 확인하세요**:

```
📍 로그 1: "Total LanesInfo: X"
   ✅ 3이어야 함
   ❌ 0이면 → LanesInfo가 비어있음
   ❌ 다른 수면 → 설정 확인 필요

📍 로그 2: "Processing Lane: X"
   ✅ 3번 나타나야 함 (0, 1, 2)
   ❌ 나타나지 않으면 → LanesInfo 구조 문제

📍 로그 3: "Team1 Towers: X"
   ✅ 3이어야 함
   ❌ 0이면 → Team1TowerPositions가 비어있음

📍 로그 4: "Team1 Tower spawned at..."
   ✅ 여러 번 나타나야 함
   ❌ 없으면 → SpawnActor 실패
```

### 2단계: 특정 로그 필터링

콘솔에서:
```
SpawnStructures
```

로 필터링하면 관련 로그만 볼 수 있습니다.

### 3단계: 에러 로그 확인

```
Failed to spawn
LanesInfo is empty
Mid Lane not found
```

이런 에러가 있으면 그 원인을 다룹니다.

---

## 🔧 해결 방법

### 방법 A: 에디터에서 자동 설정 사용 (추천)

```
MapManager Details 패널:
→ LanesInfo를 비워둔다 (Num = 0)
→ 게임 실행
→ SetupDefaultLaneInfo()가 자동으로 호출되어 3개 라인 생성
```

### 방법 B: 에디터에서 수동 설정

LanesInfo에 3개를 추가할 때:

**각 아이템마다**:

1. **LaneType 설정**
   ```
   [0] Top
   [1] Mid
   [2] Bottom
   ```

2. **Team1TowerPositions 설정** (3개)
   ```
   [0] (1200, 1200, 0) 또는 적절한 값
   [1] (600, 600, 0)
   [2] (0, 0, 0)
   ```

3. **Team2TowerPositions 설정** (3개)
   ```
   [0] (-1200, -1200, 0)
   [1] (-600, -600, 0)
   [2] (0, 0, 0)
   ```

4. **Team1CommandCenterPosition 설정**
   ```
   (-1500, 0, 0)  // Mid Lane에만 적용
   ```

5. **Team2CommandCenterPosition 설정**
   ```
   (1500, 0, 0)  // Mid Lane에만 적용
   ```

---

## 📝 권장사항

### ✅ 가장 간단한 방법:

**LanesInfo를 비워두고 자동 설정 사용**

```
1. MapManager를 레벨에 배치
2. Details → LanesInfo → 비우기 (Remove all)
3. 게임 실행
4. SetupDefaultLaneInfo()가 자동으로 동작
5. 3개 라인 + 타워 + 커맨드 센터 자동 생성
```

---

## 📊 로그 출력 흐름도

```
AAOSGameMode::StartGame()
    ↓
AAOSGameMode::InitializeStructures()
    ↓
AAOSGameMode::InitializeMapManager()
    ├→ MapManager 찾기 또는 생성
    └→ MapManager->InitializeMap() 호출
        ├→ LanesInfo.Num() == 0 ?
        │  ├─ YES → SetupDefaultLaneInfo()
        │  └─ NO → 기존 LanesInfo 사용
        └→ MapManager->SpawnStructures() 호출
            ├→ "=== SpawnStructures Started ===" ✅
            ├→ 타워 생성 루프
            │  └→ "Team1 Tower spawned at..." ✅
            ├→ 커맨드 센터 생성
            │  └→ "Command Center spawned at..." ✅
            └→ "=== SpawnStructures Complete ===" ✅
```

---

## 🎯 다음 단계

1. **빌드 수행**
   ```
   C:\UnrealProject\TDProject\Binaries\Win64\TDProject.exe 실행
   (이미 빌드됨: 18.25초)
   ```

2. **게임 실행 후 콘솔 확인**
   ```
   Ctrl+`
   "SpawnStructures" 또는 "LanesInfo" 검색
   ```

3. **콘솔 로그 결과에 따라**:
   - ✅ 모든 로그 정상 → 타워가 있어야 함
   - ❌ "LanesInfo is empty" → LanesInfo 설정 확인
   - ❌ "Failed to spawn" → 타워 생성 실패 (다른 이유)

---

**빌드 상태**: ✅ 완료 (18.25초)
**다음**: 에디터 실행 후 콘솔 로그 확인!
