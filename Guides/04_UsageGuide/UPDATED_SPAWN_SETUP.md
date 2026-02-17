# 업데이트된 스폰 포인트 설정 가이드

## 🔄 변경 사항

### Lane 설정 추가됨! ✨

**AAOSSpawnPoint에 다음 4가지 설정이 있습니다:**

```
1. Team (팀 선택)
   - Team1 (Red)
   - Team2 (Blue)

2. Lane (라인 선택)
   - Top Lane
   - Mid Lane
   - Bottom Lane

3. Spawn Index (같은 팀/라인 내 순서)
   - 0, 1, 2, 3...

4. bSpawnEnabled (스폰 활성화) ← NEW! (2026-02-17 추가)
   - true (기본값): 캐릭터를 자동 스폰
   - false: 이 스폰 포인트에서 캐릭터를 스폰하지 않음
   - 디버깅 시 특정 스폰 포인트만 활성화하여 로그를 쉽게 확인할 때 유용
```

## 레벨 에디터에서 설정하는 방법

### 스폰 포인트 배치 단계

#### 1️⃣ Team1 (Red) - Top Lane

**첫 번째 Top 스폰 포인트:**
- **Actor**: AAOSSpawnPoint
- **Name**: `SP_Team1_Top_0`
- **Location**: `(1500, 1500, 100)`
- **Details 설정:**
  ```
  Team: Team1 ✓
  Lane: Top ✓ (새로 추가됨)
  Spawn Index: 0
  ```

**두 번째 Top 스폰 포인트:**
- **Name**: `SP_Team1_Top_1`
- **Location**: `(1300, 1300, 100)`
- **Details 설정:**
  ```
  Team: Team1 ✓
  Lane: Top ✓
  Spawn Index: 1
  ```

#### 2️⃣ Team1 (Red) - Mid Lane

**첫 번째 Mid 스폰 포인트:**
- **Name**: `SP_Team1_Mid_0`
- **Location**: `(1500, 0, 100)`
- **Details 설정:**
  ```
  Team: Team1
  Lane: Mid ✓ (새로 추가됨)
  Spawn Index: 0
  ```

**두 번째 Mid 스폰 포인트:**
- **Name**: `SP_Team1_Mid_1`
- **Location**: `(1300, 0, 100)`
- **Details 설정:**
  ```
  Team: Team1
  Lane: Mid
  Spawn Index: 1
  ```

#### 3️⃣ Team1 (Red) - Bottom Lane

**첫 번째 Bottom 스폰 포인트:**
- **Name**: `SP_Team1_Bottom_0`
- **Location**: `(1500, -1500, 100)`
- **Details 설정:**
  ```
  Team: Team1
  Lane: Bottom ✓ (새로 추가됨)
  Spawn Index: 0
  ```

**두 번째 Bottom 스폰 포인트:**
- **Name**: `SP_Team1_Bottom_1`
- **Location**: `(1300, -1300, 100)`
- **Details 설정:**
  ```
  Team: Team1
  Lane: Bottom
  Spawn Index: 1
  ```

#### 4️⃣ Team2 (Blue) - Top Lane (반대편)

**첫 번째 Top 스폰 포인트:**
- **Name**: `SP_Team2_Top_0`
- **Location**: `(-1500, -1500, 100)`
- **Details 설정:**
  ```
  Team: Team2 ✓
  Lane: Top ✓ (새로 추가됨)
  Spawn Index: 0
  ```

**두 번째 Top 스폰 포인트:**
- **Name**: `SP_Team2_Top_1`
- **Location**: `(-1300, -1300, 100)`
- **Details 설정:**
  ```
  Team: Team2
  Lane: Top
  Spawn Index: 1
  ```

#### 5️⃣ Team2 (Blue) - Mid Lane

**첫 번째 Mid 스폰 포인트:**
- **Name**: `SP_Team2_Mid_0`
- **Location**: `(-1500, 0, 100)`
- **Details 설정:**
  ```
  Team: Team2
  Lane: Mid
  Spawn Index: 0
  ```

**두 번째 Mid 스폰 포인트:**
- **Name**: `SP_Team2_Mid_1`
- **Location**: `(-1300, 0, 100)`
- **Details 설정:**
  ```
  Team: Team2
  Lane: Mid
  Spawn Index: 1
  ```

#### 6️⃣ Team2 (Blue) - Bottom Lane

**첫 번째 Bottom 스폰 포인트:**
- **Name**: `SP_Team2_Bottom_0`
- **Location**: `(-1500, 1500, 100)`
- **Details 설정:**
  ```
  Team: Team2
  Lane: Bottom
  Spawn Index: 0
  ```

**두 번째 Bottom 스폰 포인트:**
- **Name**: `SP_Team2_Bottom_1`
- **Location**: `(-1300, 1300, 100)`
- **Details 설정:**
  ```
  Team: Team2
  Lane: Bottom
  Spawn Index: 1
  ```

## 🔑 핵심: Lane 설정의 중요성

### 왜 Lane을 설정해야 하나?

```
GetNearestSpawnPoint(Team, Lane) 함수가 다음을 수행합니다:

1. 해당 팀의 스폰 포인트 중에서
2. 해당 라인의 스폰 포인트를 찾아서
3. 사용 가능한 첫 번째 포인트를 반환

예시:
  GetNearestSpawnPoint(Team1, Top)
    → SP_Team1_Top_0 반환 (첫 번째 호출)
    → SP_Team1_Top_1 반환 (두 번째 호출)
    → null 반환 (세 번째 호출 - 모두 점유 상태)
```

## 설정 체크리스트

### Team1 (Red)
- [ ] **Top Lane** (2개)
  - [ ] SP_Team1_Top_0: Team=Team1, Lane=Top, Index=0
  - [ ] SP_Team1_Top_1: Team=Team1, Lane=Top, Index=1

- [ ] **Mid Lane** (2개)
  - [ ] SP_Team1_Mid_0: Team=Team1, Lane=Mid, Index=0
  - [ ] SP_Team1_Mid_1: Team=Team1, Lane=Mid, Index=1

- [ ] **Bottom Lane** (2개)
  - [ ] SP_Team1_Bottom_0: Team=Team1, Lane=Bottom, Index=0
  - [ ] SP_Team1_Bottom_1: Team=Team1, Lane=Bottom, Index=1

### Team2 (Blue)
- [ ] **Top Lane** (2개)
  - [ ] SP_Team2_Top_0: Team=Team2, Lane=Top, Index=0
  - [ ] SP_Team2_Top_1: Team=Team2, Lane=Top, Index=1

- [ ] **Mid Lane** (2개)
  - [ ] SP_Team2_Mid_0: Team=Team2, Lane=Mid, Index=0
  - [ ] SP_Team2_Mid_1: Team=Team2, Lane=Mid, Index=1

- [ ] **Bottom Lane** (2개)
  - [ ] SP_Team2_Bottom_0: Team=Team2, Lane=Bottom, Index=0
  - [ ] SP_Team2_Bottom_1: Team=Team2, Lane=Bottom, Index=1

## 빠른 설정 팁

### Details 패널에서 한눈에 보기
```
각 스폰 포인트를 선택하면 Details에서 이 4가지가 보입니다:

┌─────────────────────────────────┐
│ AOS|Spawn                       │
├─────────────────────────────────┤
│ Team           ▼ (Team1/Team2) │
│ Lane           ▼ (Top/Mid/Bot) │
│ Spawn Index    0 (정수)        │
│ Spawn Enabled  ☑ (체크박스)    │
├─────────────────────────────────┤
│ (다른 설정들...)                │
└─────────────────────────────────┘

디버깅 팁: 캐릭터가 8개 동시에 스폰되면 로그 확인이 어려움
→ 테스트할 스폰 포인트 1개만 bSpawnEnabled=true로 설정
→ 나머지는 false로 비활성화
```

### 드래그 + 설정 반복
1. AAOSSpawnPoint를 레벨에 드래그 → 배치
2. Details에서 Team 설정
3. Details에서 Lane 설정 ← 이제 보임!
4. Details에서 Spawn Index 설정
5. 반복 (총 12개)

## 예시: 캐릭터 배치 시나리오

### 시나리오: Top 2 / Mid 1 / Bottom 1

```
플레이어 배치: [Top, Top, Mid, Bottom]

스폰 프로세스:

1번 캐릭터 (라인: Top)
  → GetNearestSpawnPoint(Team1, Top)
  → SP_Team1_Top_0 할당 ✓

2번 캐릭터 (라인: Top)
  → GetNearestSpawnPoint(Team1, Top)
  → SP_Team1_Top_1 할당 ✓ (0은 이미 점유)

3번 캐릭터 (라인: Mid)
  → GetNearestSpawnPoint(Team1, Mid)
  → SP_Team1_Mid_0 할당 ✓

4번 캐릭터 (라인: Bottom)
  → GetNearestSpawnPoint(Team1, Bottom)
  → SP_Team1_Bottom_0 할당 ✓
```

## 콘솔 로그 확인

### 게임 실행 후 콘솔에서 이런 메시지가 보여야 합니다:

```
LogTemp Warning: Character spawned at lane: 0  (Top)
LogTemp Warning: Character spawned at lane: 0  (Top)
LogTemp Warning: Character spawned at lane: 1  (Mid)
LogTemp Warning: Character spawned at lane: 2  (Bottom)
```

만약 다음 에러가 보이면:
```
LogTemp Error: No available spawn point for Team X, Lane Y
```
→ 해당 라인의 스폰 포인트 설정을 확인하세요!

## 이전 버전과의 차이점

| 항목 | 이전 | 현재 |
|------|------|------|
| Team 설정 | ✓ | ✓ |
| Lane 설정 | ✗ | ✓ **NEW!** |
| Spawn Index | ✓ | ✓ |
| GetNearestSpawnPoint | 팀만 고려 | **팀 + 라인 고려** |
| 스폰 정확도 | 중간 | **높음** |

---

**요약**: 스폰 포인트를 배치할 때 **Team, Lane, Spawn Index** 3가지를 모두 설정하세요.
디버깅 시에는 **bSpawnEnabled**를 활용하여 특정 스폰 포인트만 활성화할 수 있습니다.

---

**최종 업데이트**: 2026-02-17
