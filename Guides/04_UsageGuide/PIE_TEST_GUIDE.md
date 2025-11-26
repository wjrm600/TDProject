# PIE 테스트 가이드 - 스폰 포인트에 캐릭터 배치

## 목표
PIE(Play In Editor)를 실행했을 때 각 스폰 포인트에 캐릭터들이 자동으로 배치되는 것을 확인합니다.

## 전체 설정 프로세스

### 1단계: 새 레벨 생성

1. **Unreal Editor에서 File → New Level**
2. **Empty Level** 선택
3. 레벨을 저장 (예: AOS_TestLevel)

### 2단계: 기본 요소 배치

#### 2-1) AOSGameMode 설정
1. **World Settings** 열기 (Window → World Settings)
2. **Game Mode Override**: `AOSGameMode` 선택 (또는 `AOSGameMode_C` 블루프린트)

#### 2-2) 조명 및 바닥 추가
1. **Content Browser**에서 기본 머티리얼/메시 추가
2. 바닥 평면 배치 (그리드 크기: 5000 x 5000)

### 3단계: 스폰 포인트 배치 (가장 중요!)

**총 12개의 스폰 포인트 필요** (팀당 6개, 라인당 2개)

#### 3-1) Team1 (Red) - Top Lane 스폰 포인트

1. **Place Actor** 탭에서 `AAOSSpawnPoint` 검색
2. 레벨에 드래그하여 배치

**첫 번째 Top 스폰 포인트:**
- **Name**: SP_Team1_Top_0
- **Location**: `(1500, 1500, 100)`
- **Details 설정:**
  - Team: `Team1`
  - Spawn Index: `0`

**두 번째 Top 스폰 포인트:**
- **Name**: SP_Team1_Top_1
- **Location**: `(1300, 1300, 100)`
- **Details 설정:**
  - Team: `Team1`
  - Spawn Index: `1`

#### 3-2) Team1 (Red) - Mid Lane 스폰 포인트

**첫 번째 Mid 스폰 포인트:**
- **Name**: SP_Team1_Mid_0
- **Location**: `(1500, 0, 100)`
- **Details 설정:**
  - Team: `Team1`
  - Spawn Index: `0`

**두 번째 Mid 스폰 포인트:**
- **Name**: SP_Team1_Mid_1
- **Location**: `(1300, 0, 100)`
- **Details 설정:**
  - Team: `Team1`
  - Spawn Index: `1`

#### 3-3) Team1 (Red) - Bottom Lane 스폰 포인트

**첫 번째 Bottom 스폰 포인트:**
- **Name**: SP_Team1_Bottom_0
- **Location**: `(1500, -1500, 100)`
- **Details 설정:**
  - Team: `Team1`
  - Spawn Index: `0`

**두 번째 Bottom 스폰 포인트:**
- **Name**: SP_Team1_Bottom_1
- **Location**: `(1300, -1300, 100)`
- **Details 설정:**
  - Team: `Team1`
  - Spawn Index: `1`

#### 3-4) Team2 (Blue) - Top Lane 스폰 포인트 (반대편)

**첫 번째 Top 스폰 포인트:**
- **Name**: SP_Team2_Top_0
- **Location**: `(-1500, -1500, 100)`
- **Details 설정:**
  - Team: `Team2`
  - Spawn Index: `0`

**두 번째 Top 스폰 포인트:**
- **Name**: SP_Team2_Top_1
- **Location**: `(-1300, -1300, 100)`
- **Details 설정:**
  - Team: `Team2`
  - Spawn Index: `1`

#### 3-5) Team2 (Blue) - Mid Lane 스폰 포인트

**첫 번째 Mid 스폰 포인트:**
- **Name**: SP_Team2_Mid_0
- **Location**: `(-1500, 0, 100)`
- **Details 설정:**
  - Team: `Team2`
  - Spawn Index: `0`

**두 번째 Mid 스폰 포인트:**
- **Name**: SP_Team2_Mid_1
- **Location**: `(-1300, 0, 100)`
- **Details 설정:**
  - Team: `Team2`
  - Spawn Index: `1`

#### 3-6) Team2 (Blue) - Bottom Lane 스폰 포인트

**첫 번째 Bottom 스폰 포인트:**
- **Name**: SP_Team2_Bottom_0
- **Location**: `(-1500, 1500, 100)`
- **Details 설정:**
  - Team: `Team2`
  - Spawn Index: `0`

**두 번째 Bottom 스폰 포인트:**
- **Name**: SP_Team2_Bottom_1
- **Location**: `(-1300, 1300, 100)`
- **Details 설정:**
  - Team: `Team2`
  - Spawn Index: `1`

### 4단계: 캐릭터 블루프린트 생성

#### 4-1) 블루프린트 생성
1. **Content Browser** → **Create** → **Blueprint Class**
2. **Parent Class**: `AAOSCharacter` 선택
3. **이름**: `BP_AOSCharacter`

#### 4-2) 블루프린트 설정
1. **BP_AOSCharacter** 열기
2. **Default Values** 탭에서:
   - **Capsule Component Size**: (Radius: 40, Height: 88)
   - **Skeletal Mesh**: 기본 인간형 메시 선택 (또는 임시로 비움)
   - **Movement Speed**: `600`

3. **Compile** → **Save**

### 5단계: 캐릭터 배치 (레벨에)

#### 5-1) Team1 캐릭터 2마리
1. **BP_AOSCharacter**를 레벨에 드래그
2. **첫 번째 캐릭터:**
   - **Name**: Char_Team1_1
   - **Location**: `(1400, 800, 100)`
   - **Details:**
     - Team: `Team1`
     - AssignedLane: `Top`

3. **두 번째 캐릭터:**
   - **Name**: Char_Team1_2
   - **Location**: `(1400, 0, 100)`
   - **Details:**
     - Team: `Team1`
     - AssignedLane: `Mid`

#### 5-2) Team2 캐릭터 2마리
1. **세 번째 캐릭터:**
   - **Name**: Char_Team2_1
   - **Location**: `(-1400, -800, 100)`
   - **Details:**
     - Team: `Team2`
     - AssignedLane: `Top`

2. **네 번째 캐릭터:**
   - **Name**: Char_Team2_2
   - **Location**: `(-1400, 0, 100)`
   - **Details:**
     - Team: `Team2`
     - AssignedLane: `Mid`

### 6단계: 게임 시작 로직 연결 (블루프린트)

#### 6-1) Level Blueprint 열기
1. **레벨 창** → **Blueprints** → **Open Level Blueprint**

#### 6-2) 게임 시작 이벤트 만들기
```blueprintflow
Event BeginPlay
  ├─ Get Game Mode (Cast to AOSGameMode)
  └─ Start Game (호출)
```

**상세 블루프린트:**
1. **Event BeginPlay** 노드 추가
2. **Get Game Mode** 노드 추가
3. **Cast to AOSGameMode** 노드 추가
4. **Start Game** 함수 호출
5. **Compile** → **Save**

### 7단계: PIE 테스트 실행

1. **Play** 버튼 클릭 (또는 Alt+P)
2. **관찰 내용:**

```
예상 결과:

게임 시작 후 약 1초:
  Team1
  ├─ Char_Team1_1 → SP_Team1_Top_0 (1500, 1500, 100)으로 이동
  └─ Char_Team1_2 → SP_Team1_Mid_0 (1500, 0, 100)으로 이동

  Team2
  ├─ Char_Team2_1 → SP_Team2_Top_0 (-1500, -1500, 100)으로 이동
  └─ Char_Team2_2 → SP_Team2_Mid_0 (-1500, 0, 100)으로 이동

게임 진행 중:
  └─ 각 캐릭터가 할당된 라인을 따라 순찰 시작
```

## 트러블슈팅

### 캐릭터가 스폰 포인트로 이동하지 않음

**원인 확인:**
1. ✅ **게임 모드 설정 확인**
   - World Settings에서 Game Mode가 `AOSGameMode`로 설정되었는지 확인

2. ✅ **스폰 포인트 설정 확인**
   - 각 스폰 포인트의 Team이 올바른지 확인
   - Spawn Index가 0부터 시작하는지 확인

3. ✅ **캐릭터 라인 설정 확인**
   - 캐릭터의 AssignedLane이 설정되었는지 확인
   - 해당 라인에 충분한 스폰 포인트가 있는지 확인

4. ✅ **AI 컨트롤러 확인**
   - `AAOSCharacter`의 AIControllerClass가 `AAOSAIController_C`로 설정되었는지 확인

### 캐릭터들이 겹침

**해결책:**
- 스폰 포인트 간 거리를 200+ units으로 증가
- Spawn Index 재정렬 (0, 1, 2, 3 순서대로)

### 게임이 시작되지 않음

**확인 사항:**
1. Level Blueprint에 Event BeginPlay → Start Game이 연결되었는지 확인
2. Compile 에러가 있는지 확인
3. 콘솔(`) 열어서 에러 메시지 확인

## 간단한 테스트 버전 (빠른 테스트용)

최소한의 요소로 빠르게 테스트하려면:

```
레벨 구성:
  └─ AOSGameMode (자동)
  └─ 스폰 포인트 4개 (Team1 Mid 2개, Team2 Mid 2개)
  └─ 캐릭터 2개 (Team1 1마리, Team2 1마리)
  └─ Level Blueprint (Event BeginPlay → Start Game)
```

## 다음 단계

성공적으로 PIE에서 캐릭터가 스폰되는 것을 확인한 후:

1. **타워 및 커맨드 센터 추가**
   - `AAOSStructure` 배치

2. **카메라 설정**
   - 플레이어가 게임을 볼 수 있는 카메라 설정

3. **UI 추가**
   - 게임 타이머 표시
   - 팀 정보 표시

4. **네트워크 멀티플레이 구현**
   - Replication 설정

---

**팁**: 각 스폰 포인트 위에 텍스트 렌더링 액터를 배치하면 어떤 스폰 포인트인지 시각적으로 확인하기 쉬워집니다!
