# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

**TDProject** is an Unreal Engine 5.7 MOBA-style (AOS - Auto Oriented Strategy) game with 3-lane tower defense mechanics. The project implements AI-controlled characters that push lanes, attack towers sequentially, and compete to destroy the enemy Command Center.

## 작업 타임라인 자동 갱신 (필수)

발표·회고용 작업 이력 파일: **`Guides/05_ProgressLog/TIMELINE.md`**

### 갱신 트리거 — 다음 시점에 항목을 추가/갱신한다
1. **의미 있는 작업 완료 시**: feat/fix/refactor/perf 류 커밋 직후 (사소한 오타·포맷팅 제외)
2. **트러블슈팅 원인 확정 + 해결책 적용 시**: 환경/도구/네트워크/엔진 이슈 포함
3. **새 아키텍처/시스템 도입 결정 시점**: 도입 시작 시점에 "진행 중" 섹션에, 완료 시 본문으로 이동
4. **외부에서 발견한 비호환/제약**: UE 엔진 동작·서드파티 드라이버·플러그인 한계 등

### 항목 형식 (필수 4블록 + 선택)
```
## YYYY-MM-DD — 한 줄 제목

**작업 내용**: 핵심 변경 사항 bullet
**문제점**: 막혔던 지점·원인 (없으면 생략)
**해결 방법**: 어떻게 풀었는가
**결과**: 영향 범위 + 관련 커밋 해시
(선택) 관련 가이드: Guides/... 링크
```

### 운영 원칙
- 항목은 **시간 순(오래된 것 위)** — 발표 시 스토리텔링 흐름 유지
- 진행 중인 작업은 파일 하단 `## 진행 중` 섹션에 임시로 → 완료 시 본문으로 이동
- 동일 작업 세션 내 다수 갱신 지양, **의미 단위**로 묶어 한 항목으로 작성
- 커밋 해시는 `git log --pretty=format:"%h" -1` 로 확정 후 기재
- 갱신 누락 시 다음 세션의 Claude 가 git log + 코드 diff 로 역으로 재구성하여 추가

## Build Commands

### Building the Project

엔진 경로는 머신마다 다르므로 환경변수 `UE_ROOT` 와 `<PROJECT_ROOT>` 플레이스홀더를 사용합니다. 등록 절차는 [`Mcp_Tools/README.md` §1-1](Mcp_Tools/README.md) 참고.

```powershell
# Build with Unreal Engine 5.7 (PowerShell)
& "$env:UE_ROOT\Engine\Build\BatchFiles\Build.bat" `
    TDProject Win64 Development `
    -Project="<PROJECT_ROOT>\TDProject.uproject"
```

**Note**: This project uses Visual Studio 2026 and Unreal Build Accelerator (UBA).

### Opening in Editor

Open `TDProject.uproject` in Unreal Editor 5.7. The project will compile automatically if needed.

### Hot Reload

Use the Unreal Editor's hot reload feature (Ctrl+Alt+F11) for quick C++ changes without full rebuild.

### MCP 서버 셋업 (Claude 연동)

이 프로젝트는 두 개의 MCP 서버를 사용합니다:
- `unreal-engine` — 에디터 자동화 (`Plugins/McpAutomationBridge`, `localhost:3000`)
- `unreal-rag` — C++ 코드 RAG 검색 (`Mcp_Tools/ue_rag_mcp.py`)

새 컴퓨터에서 똑같은 환경을 재현하려면 **`Mcp_Tools/README.md`** 를 따라 진행하세요.
설정 파일 템플릿: `Mcp_Tools/claude_desktop_config.example.json`

## Architecture Overview

### Core Game Loop

The game follows a MOBA-style flow where AI-controlled characters automatically push lanes toward enemy structures:

1. **Spawn**: Characters spawn at designated spawn points (12 total: 2 teams × 3 lanes × 2 spawn points per lane)
2. **Lane Push**: Characters follow a **waypoint queue system** to move sequentially through structures
3. **Combat**: Characters detect and engage enemies (characters or towers) within range
4. **Victory**: Game ends when a team's Command Center is destroyed

### Waypoint Queue System (Critical Architecture)

The AI movement system uses a **waypoint queue** pattern, not simple pathfinding. This is the most important architectural decision in the codebase.

**How it works**:
- Each AI character builds a queue of structures to visit in order
- Movement order: Friendly Towers (closest to spawn first) → Enemy Towers (closest to spawn first) → Enemy Command Center
- The character progresses through the queue, automatically skipping destroyed waypoints
- Combat interrupts waypoint movement when enemies are detected

**Key files**:
- `AOSAIController::BuildWaypointQueue()` - Constructs the waypoint sequence
- `AOSAIController::GetNextTargetLocation()` - Returns current waypoint, skips destroyed ones
- `AOSAIController::MoveTowardsTarget()` - Increments waypoint index on arrival

**Why this matters**: Don't try to "fix" the AI by adding pathfinding or navigation mesh features without understanding this system. The waypoint queue ensures MOBA-style lane pushing behavior.

### Map and Lane System

**3-Lane Structure**:
- Top, Mid, Bottom lanes
- Each lane has two teams pushing toward each other
- Managed by `AOSMapManager` which stores lane configurations in `FLaneInfo` structs

**Structure Placement**:
- **Towers**: 3 per team per lane (9 towers per team total)
- **Command Centers**: 1 per team (team-based, not lane-based)
- Tower positions are defined in `LanesInfo` array but actual spawned instances are tracked in `AllTowers` array

**Critical distinction**:
- `LanesInfo` = Editor configuration (what you set in the Blueprint) — 타워 좌표만 보유
- `AllTowers` = Runtime spawned instances (actual actors in the world)
- Always use `AllTowers` for runtime logic, `LanesInfo` for spawning

**라인 시작 위치 — SpawnPoint 가 단일 진실 공급원**:
- `FLaneInfo` 는 더 이상 `Team1StartPosition` / `Team2StartPosition` 을 가지지 않음 (제거됨)
- 라인 시작 = `(Team, Lane)` 매칭되는 `AAOSSpawnPoint` 의 `GetActorLocation()`
- `AAOSMapManager::GetLaneStartPosition(Lane, Team)` 시그니처는 유지되지만 내부가
  `GameMode->GetNearestSpawnPoint(Team, Lane)->GetActorLocation()` 으로 동작 (DS 가드: 클라이언트는 ZeroVector)
- AIController 는 `LaneStartPosition` 을 캐싱할 때 MapManager 우회로
  `ControlledCharacter->GetActorLocation()` 사용 (캐릭터가 막 SpawnPoint 에서 스폰된 직후)

### Team and Lane Enums

```cpp
// Defined in AOSGameMode.h
EAOSTeam: Team1, Team2
EAOSLane: Top, Mid, Bottom
```

These enums are used throughout the codebase for team/lane identification.

### Character Lifecycle

1. **Spawn**: `AOSSpawnPoint` creates character using `SpawnCharacter()`
2. **Initialization**: `InitializeCharacter()` sets team and lane
3. **AI Assignment**: `AOSAIController` automatically possesses character
4. **Deployment**: `DeployToLane()` triggers `StartDeployment()` on AI controller
5. **Waypoint Building**: AI builds its waypoint queue
6. **Behavior Loop**: `UpdateAIBehavior()` runs every tick to check for enemies or continue moving
7. **Death**: `OnCharacterDeath()` - hide mesh, disable collision, notify GameMode, `Destroy()` after 2s (no respawn)

### Core Classes

**AOSGameMode** - Game state management, victory conditions
- Manages game state transitions (Preparation → GameRunning → GameEnded)
- Not heavily used in current implementation (auto-spawn system doesn't require preparation phase)

**AOSMapManager** - Map layout and structure spawning
- Spawns all towers and command centers at `BeginPlay()`
- Provides lane information to AI controllers
- Critical methods: `GetTowersInLane()`, `GetCommandCenter()`

**AOSSpawnPoint** - Character spawning
- Auto-spawns characters at runtime
- 12 spawn points needed for full game (2 teams × 3 lanes × 2 per lane)
- Each has Team, Lane, and Index properties

**AOSCharacter** - AI-controlled character base class
- Health, attack, movement properties
- HP bar widget component (Screen Space, above character head)
- Death handling: hide mesh, disable collision, notify GameMode, Destroy after 2s (no respawn)

**AOSAIController** - AI behavior and movement
- **Most complex class in the project**
- Implements waypoint queue system
- Handles enemy detection, combat (characters AND structures), and lane pushing
- `AttackStructure()` - moves toward structure, attacks when in range
- Uses `Tick()` for behavior updates (not Behavior Trees or State Trees)

**AOSStructure** - Base class for towers and command centers
- Health management (Tower: 1000, CommandCenter: 5000)
- Auto-attack system: AttackRange=200, DetectionRange=400
- HP bar widget component (Screen Space, above structure)
- Destruction: hide mesh, disable detection, hide HP bar, stop Tick

**AOSHealthBarWidget** - HP bar UI (UI/AOSHealthBarWidget.h/cpp)
- Inherits UUserWidget, used as 3D world widget
- Binds to `HealthProgressBar` via `BindWidget` meta
- Team color: Team1=Red, Team2=Blue
- Requires Widget Blueprint `WBP_HealthBar` created in editor

## AI: State Tree Architecture (Phase 6)

AI 행동 결정은 **State Tree** (UE 5.4+ production-ready) 가 담당.
이전의 `AOSAIController::UpdateAIBehavior` (if/else 직접 결정) 제거됨.

### 핵심 클래스 (Phase 6)

- **`UStateTreeAIComponent`** (`Components/StateTreeAIComponent.h`)
  - `AAOSAIController` 가 `CreateDefaultSubobject` 로 부착
  - **`bStartLogicAutomatically = false`** (생성자에서 `SetStartLogicAutomatically(false)`) →
    BeginPlay 자동 시작 끔. AIController 의 `OnPossess` 가 ControlledCharacter 캐시 직후
    수동으로 `StartLogic()` 호출. (BeginPlay 시점엔 GetPawn()=null 이라 schema 의
    context actor binding 이 실패 — 자동 시작 시 `Could not find context actor of type
    AOSCharacter. StateTree will not update.` 에러)
  - StateTreeAIComponentSchema 사용 — AAIController 접근 보장
  - BP_AOSAIController 의 컴포넌트 디테일 → `StateTreeRef` 슬롯에 ST 자산 지정

- **`AAOSAIController`** (`AOSAIController.h/cpp`)
  - 기존 `UpdateAIBehavior` / `MoveTowardsTarget` / `AttackTarget` / `AttackStructure` **제거**
  - 헬퍼 메서드는 ST task 가 호출하기 위해 **public 노출**:
    - `SetCurrentTarget(AAOSCharacter*)`
    - `GetCurrentTargetCharacter()`
    - `GetCurrentWaypointStructure()`
    - `IsCurrentTargetInAttackRange()`
    - `HasArrivedAtCurrentWaypoint()`
    - `RequestMoveToCurrentTarget()` / `RequestMoveToCurrentWaypoint()`
    - `AdvanceToNextWaypoint()`
    - `FindNearestEnemy()` / `GetEffectiveAttackRange()` (이전부터 public)
  - Tick 은 race-condition 재시도 (WaypointQueue 빈 경우 재구축) + 디버그 시각화만 담당.
    행동 결정은 ST 가 자체 tick.

### Custom Tasks (`Source/TDProject/AOS/AI/AOSStateTreeTasks.h/cpp`)

모두 `FStateTreeTaskCommonBase` 상속. InstanceData 의 `Context` 카테고리로
**`TObjectPtr<AAIController>`** (base 클래스) 자동 주입 — StateTreeAIComponentSchema 가
NAME 기반("AIController") 으로 binding. **derived `AAOSAIController` 타입으로 선언하면
schema 등록 클래스와 mismatch 되어 자동 binding 실패** (engine 의 `FStateTreeMoveToTaskInstanceData`
와 동일 패턴). cpp 에서는 `Cast<AAOSAIController>` 로 derived 메서드 접근.

| Task | EnterState/Tick 동작 |
|------|----------------------|
| `FStateTreeTask_FindNearestEnemy` | 적 캐릭터 검색 → CurrentTarget 설정. 못 찾으면 FAILED |
| `FStateTreeTask_MoveToCurrentTarget` | CurrentTarget 으로 이동. 사거리 도달 시 정지하고 **RUNNING 유지** (Succeeded 반환 시 state 종료 → root 재선택 → oscillation 위험. SendAttackEvent task 가 같은 state 안에서 공격 처리) |
| `FStateTreeTask_MoveToCurrentWaypoint` | CurrentMoveTarget 으로 이동. 도착 시 **자동으로 `AdvanceToNextWaypoint()` 호출 + RUNNING 유지** (state transition 없이 task 안에서 큐 진행) |
| `FStateTreeTask_AdvanceWaypoint` | CurrentWaypointIndex++ 후 SUCCESS |
| `FStateTreeTask_SendAttackEvent` | ASC->HandleGameplayEvent(`Ability.Attack.Basic`, {Target}) — 타겟은 캐릭터/구조물 선택 |
| `FStateTreeTask_ActivateAbilityByTag` | Phase 4 스킬용. ASC->TryActivateAbilitiesByTag(Tag). **활성화 후 `State.Rooted` 보유 시 RUNNING 유지(Tick 에서 재확인) → 이동 불가 스킬이 끝날 때까지 AI 가 해당 state 에 홀드**. root 미부여(이동 가능 스킬)면 즉시 Succeeded → Design B 대로 root 재선택 |

### Custom Conditions (`Source/TDProject/AOS/AI/AOSStateTreeConditions.h/cpp`)

모두 `FStateTreeConditionCommonBase` 상속.

| Condition | TestCondition 로직 |
|-----------|--------------------|
| `FStateTreeCond_HasNearbyEnemy` | `AIController->FindNearestEnemy() != nullptr` |
| `FStateTreeCond_HealthBelowPct` | Health/MaxHealth < Threshold (instance param) |
| `FStateTreeCond_HasCooldownTag` | ASC->HasMatchingGameplayTag(Tag) — 쿨다운 active 체크 |
| `FStateTreeCond_TargetInAttackRange` | 타겟까지 거리 ≤ GetEffectiveAttackRange() |
| `FStateTreeCond_HasCurrentWaypointStructure` | 현재 웨이포인트가 적 구조물 + 미파괴 |

각 condition 의 `bInvert` 플래그로 NOT 연산 가능.

### ST 자산 작성 (사용자 작업, 시각 편집기)

**파일**: `/Game/AOS/AI/ST_AOSCharacterAI`

**에셋 디테일 설정 (필수)**:
- **스키마**: `스테이트 트리 AI 컴포넌트` (StateTreeAIComponentSchema)
- **AI 컨트롤러 클래스**: `AOSAIController`
- **컨텍스트 액터 클래스**: **`AOSCharacter`** ⚠️ (Pawn 클래스 — AIController 가 아님!)
  Schema 의 `SetContextData` 가 AIController->GetPawn() 의 IsA(ContextActorClass) 로
  Actor context 를 결정. AOSAIController 로 잘못 설정하면 schema binding 실패.

**트리 구조 (선택자 패턴, 우선순위 순)**:
```
Root (Selector — "Try Select Children In Order")
│ ⚠ Root 의 트랜지션 (필수, 우선순위 강제 전환):
│   - On Tick + cond: HasNearbyEnemy                          → Goto AttackEnemy
│   - On Tick + cond: HasCurrentWaypointStructure
│                  && TargetInAttackRange(bUseCharacter=false) → Goto AttackStructure
│ (running task 가 있는 state 는 자동 재선택 안 됨 → root transition 으로 강제)
│
├── [State] AttackEnemy
│   EnterCondition: HasNearbyEnemy
│   Tasks: FindNearestEnemy → MoveToCurrentTarget → SendAttackEvent (bTargetCurrentEnemy=true)
├── [State] UseHealSkill (Phase 4 — 추후 활성)
│   EnterCondition: HealthBelowPct(0.3) && !HasCooldownTag(Cooldown.Skill.Heal)
│   Tasks: ActivateAbilityByTag(Ability.Skill.Heal)
├── [State] AttackStructure
│   EnterCondition: HasCurrentWaypointStructure && TargetInAttackRange(bUseCurrentTargetCharacter=false)
│   Tasks: SendAttackEvent (bTargetCurrentEnemy=false)
└── [State] PushLane (Default — fallback)
    Tasks: MoveToCurrentWaypoint
    (도착/큐 advance 는 task 내부에서 자동 처리 — AdvanceWaypoint task 별도 추가 불필요)
```

### BP 연결 (사용자 작업)

1. `BP_AOSAIController` (없으면 생성: AAOSAIController 상속)
2. 디테일 패널 → StateTreeComponent → StateTreeRef → ST_AOSCharacterAI 지정
3. BP 컴파일 + 저장
4. `BP_Character` 의 AIControllerClass 가 BP_AOSAIController 가리키는지 확인

### 디버깅

**State Tree Debugger 윈도우** (UE 5.7) — Rewind Debugger 와 통합:
- 메인 에디터: **창 → 디버그 → Rewind Debugger** (또는 ST 자산 에디터 상단의 디버그 탭)
- PIE 시작 → 좌측 액터 리스트에서 인스턴스 선택 → 타임라인에 state 활성/transition 시각화
- Trace 채널 활성: `trace.start statetree` (또는 Project Settings → Trace 에서 StateTree 체크)
- `WITH_STATETREE_TRACE_DEBUGGER=1` 빌드 필요 (Editor + Development 기본 활성)

**Gameplay Debugger** (가장 빠른 방법):
- PIE 중 `'` (apostrophe) 또는 F8 키 → 화면 오버레이
- 숫자 키로 카테고리 토글 — StateTree 카테고리에서 활성 state 표시
- `Project Settings → Gameplay Debugger → Categories` 에서 StateTree 활성 필요할 수 있음

**콘솔 명령**:
- `showdebug ai` — UStateTreeAIComponent 의 GetActiveStateNames() 출력
- `gd.AIDebug.StateTree 1` — ST 활성 state 시각화
- `log LogStateTree Verbose` / `log LogAI Verbose` — 상세 로그

### Phase 6 트러블슈팅 (자주 빠지는 함정)

ST 자산 만들고 AI 가 동작 안 할 때 점검 체크리스트 — 이 3가지가 거의 모든 케이스를 커버합니다.

**1. ContextActorClass 가 AIController 로 잘못 설정**
- 증상: 빌드는 통과, ST IsRunning=true 인데 task 가 전혀 실행 안 됨
  (또는 우리 task 의 `InstanceData.AIController` 가 null 처럼 동작)
- 원인: `에셋 디테일 → 컨텍스트 액터 클래스` 가 `AOSAIController` 또는 다른 잘못된 클래스
- 수정: `AOSCharacter` (Pawn 클래스) 로 설정

**2. bStartLogicAutomatically=true 로 BeginPlay 자동 시작 (타이밍 race)**
- 증상 (PIE 로그):
  ```
  LogStateTree: Error: SetContextData: Could not find context actor of type AOSCharacter. StateTree will not update.
  LogStateTree: Error: SetContextRequirements: Missing external data requirements. StateTree will not update.
  LogStateTree: Warning: Context Requirements in UStateTreeComponent::StartTree failed. Component tick is disabled.
  [AI Controller] Possessed ... — IsRunning=false
  ```
- 원인: BeginPlay 시점엔 AIController->GetPawn() = null → schema 가 ContextActorClass(AOSCharacter) 매칭 실패
- 수정: 생성자에서 `StateTreeComponent->SetStartLogicAutomatically(false)` + OnPossess 에서
  ControlledCharacter 캐시 직후 `StateTreeComponent->StartLogic()` 수동 호출
- BP 갱신 권장: `BP_AOSAIController → StateTreeComponent → AI → Start Logic Automatically` 도 false 확인 (BP CDO override 가능)

**3. Running task 가 있는 state 는 자동 재선택 안 됨 (적 만나도 안 싸움)**
- 증상: 캐릭터가 PushLane 으로 이동 중 적이 감지 범위 안에 들어와도 AttackEnemy 로 전환 안 됨
- 원인: PushLane state 의 MoveToCurrentWaypoint task 가 RUNNING 유지 중 →
  Root selector 가 재평가하지 않음 (state tree 기본 동작)
- 수정: **Root state 에 "On Tick" 트랜지션 추가**
  - On Tick + condition `HasNearbyEnemy` → Goto AttackEnemy
  - On Tick + condition `HasCurrentWaypointStructure && TargetInAttackRange(bUseCharacter=false)` → Goto AttackStructure
- 매 tick 마다 root 가 우선순위 조건 체크해서 강제 전환

**4. InstanceData 의 AIController 타입은 base 클래스로**
- 증상: Schema 가 binding 못 함 (위 #1 과 동일 증상)
- 원인: `TObjectPtr<AAOSAIController>` 로 선언 → schema 가 등록한 base `AAIController` 와 mismatch
- 수정: `TObjectPtr<AAIController>` (base) 로 선언, cpp 에서 `Cast<AAOSAIController>` 사용
  (engine 의 `FStateTreeMoveToTaskInstanceData` 와 동일 패턴)

### Hot Reload 비호환

신규 USTRUCT (Task/Condition) 추가 → **풀 리빌드 필수**.

## GAS (Gameplay Ability System) Architecture

GAS 도입은 **5 Phase 마이그레이션** 으로 진행됩니다.
계획서: `C:\Users\wjrm7\.claude\plans\nested-herding-dragon.md` (참고용, 외부)

### Phase 진행 상태

| Phase | 내용 | 상태 |
|-------|------|------|
| 0 | 플러그인/모듈/태그/AbilitySystemGlobals 셋업 | ✅ 완료 |
| 1 | ASC + AttributeSet 부착 (병행 운영) | ✅ 완료 |
| 2 | Damage 흐름 GE_Damage 컷오버 | ✅ 완료 |
| 3 | 기본 공격 → GA_Attack 전환 | ✅ 완료 |
| 3.5 | 캐릭터 애니메이션 슬롯 스캐폴딩 (자산 미연결) | ✅ 완료 |
| 4 | 신규 스킬 추가 — Alex 캐릭터 4스킬 (Garen 스타일 Q/W/E/R) | ✅ 완료 (상하체 분리는 후속) |
| 5 | AOSStructure 도 ASC 통합 | ✅ 완료 |

### 모듈/플러그인 (Phase 0)

- `TDProject.uproject` — `GameplayAbilities` 플러그인 (GameplayTags/GameplayTasks 자동 활성)
- `Source/TDProject/TDProject.Build.cs` — `GameplayAbilities`, `GameplayTags`, `GameplayTasks` 의존성
- `Config/DefaultGame.ini` — `[/Script/GameplayAbilities.AbilitySystemGlobals]` 섹션
  (`bUseDebugTargetFromHud`, `+GameplayCueNotifyPaths=/Game/AOS/GAS/GameplayCues` 등)
- `Config/DefaultGameplayTags.ini` — Ability/Cooldown/State/Damage/Data 태그 계층

### 핵심 클래스 (Phase 1)

**`UAOSAbilitySystemComponent`** (`Source/TDProject/AOS/GAS/`)
- `UAbilitySystemComponent` 의 wrapper. 후속 Phase 의 확장 지점
- 캐릭터/구조물이 자체 소유 (PlayerState 미사용 — AI 캐릭터 패턴)

**`UAOSAttributeSet`** (`Source/TDProject/AOS/GAS/`)
- 속성: Health, MaxHealth, AttackPower, AttackRange, AttackSpeed, MoveSpeed, Damage(메타)
- Health/MaxHealth 등 6개는 `DOREPLIFETIME_CONDITION_NOTIFY` (REPNOTIFY_Always)
- Damage 는 메타 속성 — 리플리케이션 안 함, GE 입력 전용 (Phase 2 에서 PostGEExecute 처리)
- `PreAttributeChange`: Health 클램프 [0, MaxHealth]
- 기본값: Health=100, MaxHealth=100, AttackPower=10, AttackRange=500, AttackSpeed=1.0, MoveSpeed=600

### AOSCharacter 통합 (Phase 1+2)

- `IAbilitySystemInterface` 구현 → `GetAbilitySystemComponent()`
- 생성자: `AbilitySystemComponent` + `AttributeSet` 을 CreateDefaultSubobject
- ASC 설정: `SetIsReplicated(true)` + `SetReplicationMode(Mixed)`
- `PossessedBy` (서버) / `BeginPlay` (클라이언트) → `InitializeAbilitySystem()` →
  `ASC->InitAbilityActorInfo(this, this)` (Owner=self, Avatar=self)
- **Phase 2 컷오버**: `CurrentHealth` 멤버 + `OnRep_CurrentHealth` **제거**. Health 의 진짜 소스는 AttributeSet.
  - `GetCurrentHealth() / GetMaxHealth() / IsAlive()` → AttributeSet wrapper (BP 호환성 보존)
  - `MaxHealth` 멤버는 AttributeSet 초기값 시드로만 유지 (Phase 3 에서 제거 예정)
  - `AttackDamage / AttackRange / AttackCooldown / MovementSpeed` 도 시드로 유지 (Phase 3 에서 제거)

### Damage Flow (Phase 2)

```
[공격자 AIController] AttackTarget()
   ↓
[공격자 AOSCharacter] (Phase 3 에서 GA_Attack 으로 일원화 예정)
   ↓
[피격자 AOSCharacter::ReceiveDamage(float)] — deprecated wrapper
   ↓
ASC->MakeOutgoingSpec(UGE_Damage)
SetSetByCallerMagnitude("Data.Damage", DamageAmount)
ASC->ApplyGameplayEffectSpecToSelf(*Spec)
   ↓
[GE_Damage 인스턴트 적용] Damage += SetByCaller(Data.Damage)
   ↓
[UAOSAttributeSet::PostGameplayEffectExecute]
   - LocalDamage = GetDamage(); SetDamage(0) // 메타 리셋
   - NewHealth = clamp(OldHealth - LocalDamage, 0, MaxHealth); SetHealth(NewHealth)
   - if (NewHealth <= 0 && OldHealth > 0) → OnCharacterDeath()
   ↓
[AttributeChange Delegate] (서버/클라 양쪽)
   ↓
[AOSCharacter::OnHealthAttributeChanged] → UpdateHealthBar()
```

**HP 바 갱신**: 더 이상 `OnRep_CurrentHealth` 가 아닌 ASC 의
`GetGameplayAttributeValueChangeDelegate(GetHealthAttribute())` 콜백 사용.

**GE 클래스**: `Source/TDProject/AOS/GAS/Effects/GE_Damage.h/cpp` — C++ Default GE
(BP 자산 없이도 즉시 동작). 디자이너가 BP 로 derive 하고 싶으면 가능.

**AOSStructure**: 현재 Phase 2 미적용 (Phase 5 예정). 기존 float 기반
`ReceiveDamage` 그대로 동작.

### Phase 3: 기본 공격 GameplayAbility

**핵심 클래스 (Phase 3 신규)**

- `UGA_Attack` (`Source/TDProject/AOS/GAS/Abilities/GA_Attack.h/cpp`)
  - `InstancingPolicy = InstancedPerActor`, `NetExecutionPolicy = ServerInitiated`
  - AbilityTag: `Ability.Attack.Basic` (활성화 트리거)
  - `CooldownGameplayEffectClass = UGE_Cooldown_Attack::StaticClass()`
  - `ActivationBlockedTags`: `State.HitReact` (HitReact 중 공격 차단)
  - **AttackSpeed 적용** (Phase 3.5+ 갱신): `AttackSpeed` 속성을 추출하여
    - `MontageTask.Rate = AttackSpeed` → 몽타주 재생 속도 스케일
    - `CooldownSpec.SetSetByCallerMagnitude("Data.Duration", 1.0/AttackSpeed)` → 쿨다운 반비례
    - 안전 가드: `AttackSpeed <= 0` 이면 1.0 fallback (cooldown 무한대 / freeze 회피)
  - `ActivateAbility` (montage-driven, Phase 3.5+):
    AttackSpeed 추출 → 쿨다운 GE (1/AttackSpeed) → CommitCost → 타겟 cache →
    `PlayMontageAndWait(AttackMontage, Rate=AttackSpeed)` + `WaitGameplayEvent("AnimNotify.AttackHit")` →
    Notify 시점에 GE_Damage 적용 → 몽타주 종료 시 EndAbility
  - **Structure fallback**: 타겟이 ASC 미보유면 `Cast<AAOSStructure>` → `ReceiveDamage(float)` 직접 호출 (Phase 5 에서 fallback 제거)

- `UGE_Cooldown_Attack` (`Source/TDProject/AOS/GAS/Effects/GE_Cooldown_Attack.h/cpp`)
  - Duration = SetByCaller(`Data.Duration`) — GA_Attack 이 `1.0 / AttackSpeed` 로 set
  - 예시: AttackSpeed=1.0 → 1.0s, 2.0 → 0.5s, 0.5 → 2.0s
  - GrantedTag: `Cooldown.Attack.Basic` (다음 활성화 차단)

**AOSCharacter 통합 (Phase 3)**

- 멤버 추가: `TArray<TSubclassOf<UGameplayAbility>> StartupAbilities` (BP 에서 추가 능력 부여 가능)
- 생성자: `StartupAbilities.Add(UGA_Attack::StaticClass())`
- `PossessedBy` 에서 `GiveStartupAbilities()` 호출 → 서버가 능력 부여
- `GetAttackDamage()` → `AttributeSet->GetAttackPower()` wrapper
- `OnMoveSpeedAttributeChanged` 델리게이트 → `CharacterMovement->MaxWalkSpeed` 동기화
  (Phase 4 의 GA_Charge 가 MoveSpeed 모디파이 시 자동 반영)

**AOSAIController 변경**

- `AttackTarget` / `AttackStructure` 의 데미지 적용 부분이 `ASC->HandleGameplayEvent(...)` 로 전환:
  ```cpp
  if (ASC && !ASC->HasMatchingGameplayTag(Cooldown.Attack.Basic)) {
      FGameplayEventData EventData;
      EventData.Target = TargetActor;
      EventData.Instigator = ControlledCharacter;
      ASC->HandleGameplayEvent("Ability.Attack.Basic", &EventData);
  }
  ```
- `CurrentAttackCooldown` / `AttackCooldownDuration` 멤버 **제거** — ASC 태그가 단일 진실 공급원
- `AttackRange` 는 유지 (AI 행동 판단 — 어디까지 접근하면 공격할지)

**Damage Flow (Phase 3+5 갱신)**

```
[AIController::Tick] UpdateAIBehavior
   ↓ FindNearestEnemy / Tower
[AttackTarget / AttackStructure]
   ↓ if (!ASC->HasMatchingGameplayTag("Cooldown.Attack.Basic"))
ASC->HandleGameplayEvent("Ability.Attack.Basic", {Target, Instigator})
   ↓ 트리거
[GA_Attack::ActivateAbility]
   - 명시 Cooldown GE 적용 (Duration = 1.0/AttackSpeed 초, "Cooldown.Attack.Basic" 태그 부여)
   - DamageAmount = AttributeSet::AttackPower
   - Target IAbilitySystemInterface (Character/Structure 모두) → ApplyGameplayEffectSpecToTarget(GE_Damage, TargetASC)
   - EndAbility
   ↓
[GE_Damage 적용] → AttributeSet::PostGameplayEffectExecute → Health 차감
   - if NewHealth<=0 && OldHealth>0:
       - AAOSCharacter  → OnCharacterDeath()
       - AAOSStructure  → OnStructureDestroyed()
   ↓ (양쪽 모두)
OnHealthAttributeChanged → UpdateHealthBar
```

### Phase 5: AOSStructure 통합

**핵심 변경**

- `AAOSStructure : public AActor, public IAbilitySystemInterface`
- 캐릭터와 동일한 `UAOSAbilitySystemComponent` + `UAOSAttributeSet` 재사용 (Health/MaxHealth 만 사용)
- `CurrentHealth(Replicated)` / `OnRep_CurrentHealth` 멤버 **제거**, `MaxHealth` 는 시드로 유지
- `GetCurrentHealth() / GetMaxHealth() / IsDestroyed()` → AttributeSet wrapper (BP 호환)
- `ReceiveDamage(float)` → GE_Damage 적용 (캐릭터와 동일 패턴)
- BeginPlay 에서 `InitializeAbilitySystem()` 호출 (Pawn 이 아니라 PossessedBy 없음 — 양쪽에서 BeginPlay)
- `Initialize()` 에서 StructureType 별 MaxHealth 갱신 후 AttributeSet 재시드 (Tower=1000, CC=5000)
- `OnStructureDestroyed` → public 노출 (AttributeSet PostGEExecute 가 호출)

**AOSAttributeSet PostGameplayEffectExecute 분기 추가**
```cpp
if (NewHealth <= 0 && OldHealth > 0) {
    AActor* Owner = GetOwningActor();
    if (AAOSCharacter* Char = Cast<AAOSCharacter>(Owner))      Char->OnCharacterDeath();
    else if (AAOSStructure* Struct = Cast<AAOSStructure>(Owner)) Struct->OnStructureDestroyed();
}
```

**GA_Attack fallback 제거**: 이제 Structure 도 IAbilitySystemInterface 구현 → 모던 경로 (`ApplyGameplayEffectSpecToTarget`) 가 자동 처리. `Cast<AAOSStructure>` 분기 제거.

**구조물의 자체 공격은 그대로**: `Tower::FireAtTarget` 의 `Target->ReceiveDamage(...)` 호출은 변경 없음 (Character::ReceiveDamage 가 이미 GE_Damage wrapper). GA_Tower_Attack 미적용 — 단순 공격이라 GE 만으로 충분.

### Phase 4: 캐릭터 스킬 시스템 — Alex (Garen 스타일 Q/W/E/R)

> ⚠️ **이 섹션은 원본 Phase 4 (캐릭터당 4 C++ Ability + 4 쿨다운 GE) 구현 기록**.
> **2026-06-02 데이터 주도 마이그레이션 완료** — 이 8개 C++ 클래스(`GA_Alex_Q/W/E/R`, `GE_Cooldown_Alex_Q/W/E/R`)는 모두 **제거**되었고, `UGA_SkillBase` 기반 BP 자산(`BP_GA_Alex_*`, `BP_GE_Cooldown_Alex_*`)으로 대체됨. 아래 "스킬 데이터 주도식 — `UGA_SkillBase`" 섹션이 **현재 운영 패턴**.
> 이 섹션은 게임 스펙(스킬 수치/효과) 참고용으로만 유지.

LoL 식 **캐릭터당 3스킬 + 1궁극기** 구성. 첫 캐릭터 **Alex** 는 가렌 스킬셋.

**스킬 매핑**

| 슬롯 | 이름 | 효과 (단순화) | 쿨다운 |
|------|------|---------------|--------|
| Q | DecisiveStrike | 3s +300 MoveSpeed (`State.SpeedBoost`) + 다음 공격 1.5배 (`State.EnhancedAttack`) | 8s |
| W | Courage | 2s 받는 데미지 50% 감소 (`State.DamageShield`) | 15s |
| E | Judgment | 3s 회전, 0.5s마다 반경 250 적에 50 데미지 (6틱) | 10s |
| R | DemacianJustice | 단일 처형: 250 + (MaxHP-HP)×0.3 | 90s |

**핵심 클래스** (`Source/TDProject/AOS/GAS/`)

- `UGA_Alex_Q/W/E/R` (`Abilities/`) — 모두 `InstancedPerActor` + `ServerInitiated`, `ActivationBlockedTags=State.HitReact`
  - Q/W: 쿨다운 GE → 자기 버프 GE 적용 → `SkillMontages[tag]` 재생 (PlayMontageAndWait)
  - E: 쿨다운 → `FTimerManager` 0.5s × 6틱 → `OverlapMultiByChannel(ECC_Pawn, 반경 250)` → 적팀만 GE_Damage. `ActivationOwnedTags`에 `State.Spinning`. `EndAbility` 에서 타이머 정리
  - R: 쿨다운 → 타겟의 missing HP 비례 데미지. **타겟 fallback**: `TriggerEventData->Target` null 이면 (= ActivateAbilityByTag 경로) `AIController->GetCurrentTargetCharacter()` → `FindNearestEnemy()`
- `UGE_Cooldown_Alex_Q/W/E/R` (`Effects/`) — 고정 Duration + `Cooldown.Skill.Alex.X` 태그 (GE_Cooldown_Attack 패턴)
- `UGE_MoveSpeed_Boost` — MoveSpeed Additive +300, `State.SpeedBoost`
- `UGE_EnhancedAttack` — `State.EnhancedAttack` 마커 (modifier 없음). GA_Attack 이 데미지 시 ×1.5 + `RemoveActiveEffectsWithGrantedTags` 로 1회 소비
- `UGE_DamageShield` — `State.DamageShield`. `AOSAttributeSet::PostGameplayEffectExecute` 가 데미지 차감 분기에서 `Data.Target.HasMatchingGameplayTag(State.DamageShield)` 시 `LocalDamage *= 0.5`

**StateTree Condition 추가** (`AOSStateTreeConditions.h/cpp`)
- `FStateTreeCond_HasNearbyEnemies(MinCount, Radius)` — 반경 내 적팀 N명 (E용)
- `FStateTreeCond_TargetHealthBelowPct(Threshold)` — CurrentTarget HP% (R용)

**BP_Char_Alex 설정**: `StartupAbilities` 에 GA_Alex_Q/W/E/R 추가 + `SkillMontages` 매핑 (`Ability.Skill.Alex.Q` → `AM_Alex_Q` 등)

### 스킬 데이터 주도식 — `UGA_SkillBase` (Phase 4+ 진행 중)

위 Phase 4 의 "캐릭터당 4개 C++ Ability + 4개 쿨다운 GE" 패턴은 캐릭터·스킬 수가 늘수록 폭증. 이를 해소하기 위해 **C++ 1개 base 클래스 + BP child 자산** 패턴으로 전환 중.

**핵심 클래스** (`Source/TDProject/AOS/GAS/`)

- `UGA_SkillBase` (`Abilities/GA_SkillBase.h/cpp`) — UCLASS(Abstract). 모든 캐릭터 스킬의 부모. 데이터 주도식 UPROPERTY 다발:
  - **Identity**: `SkillIdentityTag` (Ability.Skill.* — `AbilityTags`/`ActivationOwnedTags` 에 `PostInitProperties`/`PostLoad` 가 자동 추가)
  - **Cast**: `bAllowMovementDuringCast`, `ExplicitRootDuration`(-1=자동: Periodic→Montage 길이)
  - **Cooldown**: `CooldownDuration` (`CooldownGameplayEffectClass`는 표준 슬롯)
  - **Effects**: `SelfAppliedEffects[]`, `TargetAppliedEffects[]`, `DamageGameplayEffectClass`
  - **Targeting**: `ESkillTargetType` (Self/SingleEnemy/AoE_Sphere), `AoERadius`
  - **Damage**: `BaseDamage`, `MissingHpDamageScale` (R 의 처형식 = 0.3)
  - **Periodic**: `PeriodicTickCount`, `PeriodicTickInterval` (E 의 6틱 × 0.5s)
  - 특이 데미지식은 `BlueprintNativeEvent CalculateTargetDamage(Target)` 를 BP override
  - 공통 적용: `ActivationOwnedTags += State.Casting`, `ActivationBlockedTags += State.HitReact`

- `UGE_SkillCooldown_Base` (`Effects/GE_SkillCooldown_Base.h/cpp`) — Duration GE + SetByCaller(Data.Duration). 태그 grant 는 BP child 가 `UTargetTagsGameplayEffectComponent` 로 추가 (예: `BP_GE_Cooldown_Alex_Q` 가 `Cooldown.Skill.Alex.Q`).

**ActivateAbility 통합 흐름** (모든 스킬 동일)
1. 쿨다운 GE (SetByCaller Data.Duration = CooldownDuration)
2. CommitAbilityCost
3. SelfAppliedEffects → self 일괄 적용
4. TargetType 분기:
   - Self: skip
   - SingleEnemy: TriggerEventData→Target / AIController fallback (R 패턴) → 데미지 + TargetAppliedEffects
   - AoE_Sphere: PeriodicTickCount>0 → FTimerManager 로 Interval×Count, 아니면 즉발 1회 — `OverlapMultiByChannel(ECC_Pawn)` 적팀만
5. `!bAllowMovementDuringCast` → `ApplyCastRoot(ResolveRootDuration())` (Explicit>0 ? : Periodic>0 ? Count×Interval : Montage 길이)
6. SkillMontage (`AOSCharacter::GetSkillMontage(SkillIdentityTag)`) 재생 (`PlayMontageAndWait`)
7. 몽타주 종료 또는 Periodic 마지막 tick 에서 `EndAbility` (둘 중 늦은 쪽)

**BP child 작성 패턴 (디자이너 작업)**

새 스킬 = `BP_GA_<Char>_<Slot>` 자산 1개 + `BP_GE_Cooldown_<Char>_<Slot>` 자산 1개:
- Right-click → Blueprint Class → Parent: `UGA_SkillBase` → 이름 `BP_GA_Alex_Q`
- Class Defaults 에서 UPROPERTY 채움
- 쿨다운: `BP_GE_Cooldown_Alex_Q` (parent=`UGE_SkillCooldown_Base`) → Components → TargetTagsGameplayEffectComponent → `Cooldown.Skill.Alex.Q`
- `BP_Char_Alex.StartupAbilities` 에 BP_GA_Alex_Q 추가
- 특이 로직 시: BP event graph 의 `Calculate Target Damage` override

**예시 설정값 (기존 4스킬 이식)**
| 스킬 | TargetType | 핵심 UPROPERTY |
|------|-----------|----------------|
| Q (DecisiveStrike) | Self | SelfEffects=[GE_MoveSpeed_Boost, GE_EnhancedAttack], Cooldown=8, bAllowMovement=false |
| W (Courage) | Self | SelfEffects=[GE_DamageShield], Cooldown=15, bAllowMovement=true |
| E (Judgment) | AoE_Sphere | AoERadius=250, PeriodicTickCount=6, PeriodicTickInterval=0.5, BaseDamage=50, Damage=GE_Damage, Cooldown=10, bAllowMovement=false |
| R (DemacianJustice) | SingleEnemy | BaseDamage=250, MissingHpDamageScale=0.3, Damage=GE_Damage, Cooldown=90, bAllowMovement=false |

**마이그레이션 상태** (2026-06-02 완료)
- ✅ 1단계: C++ base 작성 (`UGA_SkillBase`, `UGE_SkillCooldown_Base`)
- ✅ 2단계: BP child 8개(`BP_GA_Alex_*` + `BP_GE_Cooldown_Alex_*`) 생성 + `BP_Char_Alex.StartupAbilities` 매핑 교체
- ✅ 3단계: 기존 C++ 클래스 8개(`GA_Alex_Q/W/E/R` + `GE_Cooldown_Alex_Q/W/E/R`) 제거
- 보너스: `GA_Attack` stuck race 픽스 (State.Casting 차단 + UGA_SkillBase 가 시작 시 active Attack `CancelAbilities` + `bAllowInterruptAfterBlendOut=true`)

이제부터 새 캐릭터/스킬 추가는 **BP 자산만 작성** (C++ 빌드 불필요). `Guides/03_Implementation/SKILL_AUTHORING_GUIDE.md` 참고.

### StateTree 스킬 통합 — "Design B" (재선택 패턴)

스킬을 StateTree 에 통합할 때 핵심 함정과 채택한 패턴:

**StateTree task 와 GameplayAbility 의 수명은 분리됨**
- `FStateTreeTask_ActivateAbilityByTag` 는 `TryActivateAbilitiesByTag` 후 **즉시 Succeeded** 반환 (ability 종료를 기다리지 않음).
- GA 자체는 몽타주 완료 시 `OnMontageCompleted → EndAbility` (별도 긴 수명).
- 따라서 StateTree 는 스킬 발동 즉시 재선택 → 이동 재개. **몽타주는 DefaultSlot 에서 계속 재생 → 캡슐 이동 + 정지 캐스트 애니 = "슬라이드"** 현상.

**Design B — running state 만 재선택 트리거** (채택)
- 스킬(UseR/W/Q/E)은 task 가 즉시 완료형 → **재선택 자동 트리거 → 트랜지션 불필요**.
- RUNNING 유지 state (PushLane=MoveToCurrentWaypoint, AttackEnemy=MoveToCurrentTarget) 만 `On Tick → Root` 재선택 트랜지션 필요.
- **우선순위 = 자식 노드 순서 한 곳**: `UseR → UseW → UseQ → UseE → AttackEnemy → AttackStructure → PushLane` (PushLane 은 조건 없는 fallback 이라 **반드시 맨 마지막**).
- 새 스킬 추가 시 자식 노드만 추가 (트랜지션 변경 불필요) → 확장성 ↑.
- (대안 Design A: Root 에 per-skill On-Tick 전환 — 스킬 추가마다 전환 추가 필요, 비추천)

**트랜지션 우선순위**: 한 state 에 트랜지션 여러 개가 동시에 참일 수 있을 때 위→아래 첫 통과가 이김. Design B 에선 각 running state 가 `→ Root` 하나뿐이라 우선순위는 자식 순서가 담당.

**HasCooldownTag 는 bInvert=true**: "쿨다운 **없을 때** 사용 가능" 의미. false 면 거꾸로 (쿨다운 중에만 발동) → 스킬 영영 안 나감.

### Phase 4+: 스킬 시전 root + 상하체 분리 (Layered Animation)

위 "슬라이드" 의 근본 해결. **스킬 GA 의 플래그 1개(`bAllowMovementDuringCast`)가 이동 가능/불가를 결정**하고, 애니는 이동 여부에 따라 상하체 분리 / 전신을 자동 선택.

**스킬별 이동 가능 플래그** (`UGA_SkillBase::bAllowMovementDuringCast` — BP CDO 에서 디자이너가 조정)
- 현 BP 운영 값: **Q=false** (풀 애니메이션 holds), **W=true** (이동 시전), **E=false** (회전 중 고정), **R=false** (처형 모션 중 고정)
- `true`: root 안 함 → StateTree task 즉시 Succeeded (Design B) → 이동 재개 → ABP 가 상하체 분리.
- `false`: `UGA_SkillBase` 가 `Char->ApplyCastRoot(Duration)` 호출 → `State.Rooted` 부여 + `StopMovementImmediately()` → 캐릭터 정지 + AI 가 스킬 끝까지 홀드.

**C++ 구성 요소**
- `UGE_Rooted` (`Effects/GE_Rooted.h/cpp`) — Duration GE, `SetByCaller(Data.Duration)`, `State.Rooted` 부여 (GE_HitReact_State 패턴).
- `AAOSCharacter::ApplyCastRoot(float Duration)` — `UGE_Rooted` self 적용(Duration 전달) + `CMC->StopMovementImmediately()`. 서버 권한 가정.
- `UGA_SkillBase` (데이터 주도 부모) — 생성자에 `ActivationOwnedTags += State.Casting` (ABP 신호 공통), `ActivateAbility` 의 몽타주 직전에 `if (!bAllowMovementDuringCast && Char) Char->ApplyCastRoot(ResolveRootDuration())`.
  - `ResolveRootDuration()` = `ExplicitRootDuration > 0 ? : PeriodicTickCount > 0 ? Count*Interval : Montage 길이`
- `FStateTreeTask_ActivateAbilityByTag` — 활성화 후 `State.Rooted` 보유 시 RUNNING(EnterState+Tick), 해제 시 Succeeded. GE_Rooted 가 고정 Duration 이라 반드시 만료 → 무한 홀드 없음.
- `UAOSAnimInstance::bIsCasting` — `State.Casting` 태그 미러 (ABP 상하체 분리 트리거).

**ABP 애니 작업** (Part B — 사용자/agent-art-anim, 미완)
- 스킬 몽타주 `AM_Alex_*` 를 `DefaultSlot` → **`UpperBody` 슬롯**으로 (Attack/Death 전신용과 분리, AM_HitReact 패턴 재활용).
- ABP AnimGraph: locomotion 을 cached pose 로 저장 → 정지 시 전신 스킬(슬롯을 전신 적용), 이동 시 `Layered Blend Per Bone`(base=loco, layer=슬롯, mask=`spine_01`↑) → `Blend Poses by bool(bIsMoving)`.
- 상체 전용 자산 **불필요** — Layered Blend Per Bone 본 마스크가 전신 몽타주의 상체만 추출.

**검증**: Q → 적에게 달려가며 시전, 다리는 계속 달리고 상체만 스킬(슬라이드 없음). E/R → 시전 시 즉시 정지 + 전신 애니, 끝날 때까지 AI 가 그 자리. BP 에서 E 의 플래그를 true 로 바꾸면 이동하며 상체 분리되는지 확인.

### Attribute 초기값 세팅 패턴 (DataTable 기반)

**권장 패턴: 3개의 DataTable + float 멤버 fallback**

#### Row 타입 (`FAOSAttributeInitRow`, `Source/TDProject/AOS/GAS/Data/AOSAttributeInitData.h`)
모든 AttributeSet 속성을 담는 공통 row 구조체:
```cpp
struct FAOSAttributeInitRow : public FTableRowBase {
    float Health = 100.f;
    float MaxHealth = 100.f;
    float AttackPower = 10.f;
    float AttackRange = 500.f;
    float AttackSpeed = 1.f;     // 1/sec, 쿨다운 = 1/AttackSpeed
    float MoveSpeed = 600.f;
};
```

#### DataTable 자산 (디자이너가 에디터에서 생성)
| 자산 경로 | 사용처 |
|-----------|--------|
| `/Game/AOS/GAS/Data/DT_CharacterAttributes` | `AAOSCharacter` |
| `/Game/AOS/GAS/Data/DT_TowerAttributes` | `AAOSStructure` (Tower) |
| `/Game/AOS/GAS/Data/DT_CommandCenterAttributes` | `AAOSStructure` (CommandCenter) |

각 DT 의 row name 은 기본 `"Default"`. 캐릭터/구조물별 다른 값을 원하면 row 추가 후 BP 에서 `AttributeInitRowName` 변경.

**DT 자산 만드는 법** (에디터):
1. Content Browser → `AOS/GAS/Data` 폴더로 이동
2. 우클릭 → Miscellaneous → Data Table
3. Row Structure 선택: `AOSAttributeInitRow`
4. 이름: `DT_CharacterAttributes` (또는 `DT_TowerAttributes`, `DT_CommandCenterAttributes`)
5. 자산 열기 → row 추가, name=`Default`, 값 입력

#### 캐릭터/구조물 BP 설정
- **BP_Character** → `AOS|GAS|Init` 카테고리 → `Attribute Init Table` 에 `DT_CharacterAttributes` 지정
- **BP_Tower** → `Tower Attribute Init Table` 에 `DT_TowerAttributes`
- **BP_CommandCenter** → `Command Center Attribute Init Table` 에 `DT_CommandCenterAttributes`
  (또는 단일 BP_Structure 가 두 DT 모두 보유 — Initialize 의 `StructureType` 이 자동 분기)

#### 시드 우선순위 (`InitializeAbilitySystem` / `ApplyAttributeSeeds`)
1. **DataTable** 의 row → 디자이너 친화적 중앙 데이터 (권장)
2. **float 멤버 fallback** — `MaxHealth`, `AttackDamage`, `AttackRange`, `AttackCooldown`, `MovementSpeed`
   (DT 미설정/row 미발견 시 사용)
3. **MoveSpeed 만 예외**: BP CharacterMovement→MaxWalkSpeed override 가 우선 (BP 의 자연스러운 조정 존중)

#### 호출 시점
- **AOSCharacter**: `PossessedBy`(서버) / `BeginPlay`(클라) → `InitializeAbilitySystem()` 한 번
- **AOSStructure**: `BeginPlay` → `InitializeAbilitySystem()` (default Tower 시드) →
  `Initialize(Type, ...)` 가 호출되면 StructureType 확정 후 `ApplyAttributeSeeds()` 재호출
  (default Tower → 정확한 Tower/CC 시드로 덮어씀)

**주의**: 생성자에서 `GetCharacterMovement()->MaxWalkSpeed = MovementSpeed` 같은 강제 할당 금지.
BP override 가 적용 후에 코드가 다시 덮어쓸 위험.

### (참고) 다른 Attribute 초기화 옵션
- (구식) **GE_InitCharacter** — Instant GE 로 모든 속성 일괄 부여. Magnitude 가 정적이라 캐릭터별 다른 값 어려움. DT 패턴이 더 유연.
- (구식) **`UAbilitySystemGlobals::GlobalAttributeMetaDataTable`** — 엔진 내장 DataTable 시스템. 우리 DT 패턴과 비슷하지만 사용처가 분산됨.

### UE 5.4+ GameplayEffect Component 시스템 (필수)

UE 5.4 부터 `UGameplayEffect` 가 **`UGameplayEffectComponent` 기반**으로 리팩토링됨.
이전 방식 (`InheritableOwnedTagsContainer.Added`, `InheritableBlockedAbilityTagsContainer` 등) 은
런타임 태그는 적용되지만 **cooldown / 일부 검증 시스템에서 인식 안 됨**.

**증상**: cooldown GE 적용 시 로그에 다음 경고 + 매 frame ability 활성화:
```
LogAbilitySystem: Warning: CooldownGameplayEffectClass 'GE_*' grants no tags.
A GameplayEffect class must grant tags (Component: Grant Tags to Target Actor) to be used as cooldown.
```

**올바른 패턴 — `CreateDefaultSubobject` + `GEComponents.Add`** (생성자 안전):
```cpp
#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"

UGE_Cooldown_Attack::UGE_Cooldown_Attack()
{
    DurationPolicy = EGameplayEffectDurationType::HasDuration;
    DurationMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(1.0f));

    // ⚠️ FindOrAddComponent / AddComponent 는 NewObject 를 호출하므로
    //    CDO 생성자 안에서 호출 시 fatal error (AssertIfInConstructor).
    //    대신 CreateDefaultSubobject + GEComponents 직접 추가 패턴 사용.
    UTargetTagsGameplayEffectComponent* TagsComp =
        CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(TEXT("TargetTagsGEComp"));
    if (TagsComp)
    {
        FInheritedTagContainer TagsContainer;
        TagsContainer.Added.AddTag(FGameplayTag::RequestGameplayTag("Cooldown.Attack.Basic"));
        TagsComp->SetAndApplyTargetTagChanges(TagsContainer);
        GEComponents.Add(TagsComp);  // GEComponents 는 protected — 자식 클래스 접근 가능
    }
}
```

**Instant GE 에서는 TargetTagsComponent 사용 금지** — `IsDataValid` 가 에러.
Instant 는 태그를 ASC 에 grant 할 수 없음 (즉시 만료). 데미지 타입 분류 등은
`FGameplayEffectContextHandle` 또는 `GameplayCue` 로 처리.

다른 컴포넌트 (cooldown 외 일반 태그/blocked tags 등) 도 같은 패턴:
- `UTargetTagsGameplayEffectComponent` — Target ASC 에 태그 부여 (Duration GE 에서)
- `UAssetTagsGameplayEffectComponent` — GE 자체의 asset tags (Instant 도 OK)
- `UBlockAbilityTagsGameplayEffectComponent` — 차단할 ability tags

### 디버깅

- 콘솔: `showdebug abilitysystem` — ASC 상태/태그/속성 실시간 확인
- 로그 카테고리: `LogAbilitySystem`, `LogGameplayCue` (Verbose 권장)

### Hot Reload 비호환

ASC/AttributeSet 신규 추가, 모듈 의존성 변경 등은 **풀 리빌드 필요**.
매 Phase 시작 시 에디터 종료 후 Build.bat 실행.

## 캐릭터 애니메이션 시스템 (Phase 3.5)

캐릭터 애니메이션은 **C++ 슬롯 + BP 자산 연결** 패턴.
Phase 3.5 는 슬롯만 만든 상태이며, **자산은 디자이너가 BP_Character 디테일에서 채운다**.

### 핵심 클래스

- **`UAOSAnimInstance`** (`Source/TDProject/AOS/AOSAnimInstance.h/cpp`)
  - BlueprintReadOnly 변수: `Speed`, `Direction`, `bIsMoving`, `bIsFalling`, `bIsAttacking`, `bIsCasting`, `bIsHitReacting`, `bIsDead`
  - `NativeUpdateAnimation` 에서 `OwningCharacter->GetVelocity()` + `CachedASC->HasMatchingGameplayTag(...)` 로 매 tick 갱신
  - DS 환경: `Velocity` 는 클라에 자동 replicate → 별도 동기화 코드 없음
  - GAS 태그 미러는 정의된 태그만 (`Ability.Attack.Basic`). 미정의 태그는 Phase 4 에서 추가.

- **`UAOSAnimNotify_AttackHit`** (`Source/TDProject/AOS/Anim/AOSAnimNotify_AttackHit.h/cpp`)
  - 본 Phase: 빈 껍데기 (로깅만)
  - Phase 4 에서 `GA_Attack` 의 `PlayMontageAndWait` + `WaitGameplayEvent` 패턴과 결합 → montage-driven damage

### `AAOSCharacter` 몽타주 슬롯

```cpp
UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AOS|Animation")
TObjectPtr<UAnimMontage> AttackMontage;

UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AOS|Animation")
TObjectPtr<UAnimMontage> HitReactMontage;

UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AOS|Animation")
TObjectPtr<UAnimMontage> DeathMontage;

UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AOS|Animation")
TMap<FGameplayTag, TObjectPtr<UAnimMontage>> SkillMontages;  // Phase 4 확장

UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AOS|Animation", meta=(ClampMin="0.0"))
float RagdollSettleDuration = 2.0f;  // 사망 몽타주 종료 후 ragdoll 유지 시간(액터 destroy 까지)
```

- 슬롯이 nullptr 이면 조용히 skip — **자산 없어도 PIE 가 굴러간다.**
- `GetSkillMontage(FGameplayTag)` 로 Phase 4 의 GA_Skill_X 가 매핑된 몽타주 조회.
- `RagdollSettleDuration` 은 ragdoll 시뮬레이션이 정착할 시간 — 너무 짧으면 시체가 공중에서 사라짐.

### 트리거 흐름

| 애니메이션 | 트리거 진입점 | 호출 방식 |
|----------|---------------|-----------|
| Idle / Move | (없음 — ABP 가 `Speed`/`Direction` 변수만 읽음) | AnimGraph 자체 |
| Attack | `GA_Attack::ActivateAbility` (서버) | `Char->PlayAnimMontage(AttackMontage)` 한 줄 (자동 replicate). **데미지는 즉시 적용 그대로** — Phase 4 에서 montage-driven 으로 전환 |
| HitReact | `UAOSAttributeSet::PostGameplayEffectExecute` (서버, 데미지 차감 분기, NewHealth > 0 일 때) | `Char->Multicast_PlayHitReact()` (NetMulticast Reliable) |
| Death | `AAOSCharacter::OnCharacterDeath` (서버) | (1) `Brain.StopLogic("Character died")` — StateTree 즉시 정지 (없으면 다음 tick 의 SendAttackEvent 가 DeathMontage 를 덮어씀) → (2) `CMC->SetMovementMode(MOVE_Walking)` 강제 (NavWalking 이면 root motion 무효) → (3) `Multicast_PlayDeathMontage()` (서버+클라 각각 `PlayAnimMontage` + 타이머 → `StartRagdoll`) → (4) `SetLifeSpan(MontageLength + RagdollSettleDuration)` |
| Skill | (Phase 4) `GA_Skill_X::ActivateAbility` | `Char->PlayAnimMontage(GetSkillMontage(SkillTag))` |

### HitReact ↔ Attack 충돌 규칙 (Phase 3.5+)

두 몽타주가 같은 `DefaultSlot` 에서 재생되므로 HitReact 가 Attack 을 중간에 끊는 문제가 있었음.
또한 AI 가 HitReact 중에도 공격을 시도해서 부자연스러웠음. 다음 두 규칙으로 해결:

#### Rule A — 공격 중 HitReact 스킵

`AAOSCharacter::Multicast_PlayHitReact_Implementation` 진입 시 ASC 가 `Ability.Attack.Basic`
태그를 가지고 있으면 (= GA_Attack 활성 중) HitReact 재생을 스킵.

```cpp
if (ASC->HasMatchingGameplayTag(FGameplayTag::RequestGameplayTag("Ability.Attack.Basic")))
{
    return;  // 공격 진행 중 — hit react 무시
}
```

#### Rule B — HitReact 중 공격 차단

`State.HitReact` GameplayTag + `UGE_HitReact_State` Duration GE (`UTargetTagsGameplayEffectComponent`
로 태그 부여) + GA_Attack 의 `ActivationBlockedTags` 에 `State.HitReact` 등록.

흐름:
1. 데미지 받음 → `Multicast_PlayHitReact` 호출
2. Rule A 통과 (공격 중 아님) → 서버에서 `ApplyHitReactStateGE()` 호출
3. `UGE_HitReact_State` 적용 → `State.HitReact` 태그 부여 (Duration = HitReactMontage 길이)
4. 태그 active 동안 GA_Attack 활성화 시도 시 `ActivationBlockedTags` 에 걸려 거부됨
5. AI 의 StateTree 가 `SendAttackEvent` 를 계속 보내도 ability 가 무시 → 사실상 일시 정지
6. HitReactMontage 길이 만큼 후 GE 만료 → 태그 제거 → 공격 재개 가능

```cpp
// GA_Attack 생성자
ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag("State.HitReact"));
```

#### HitReact Duration — SetByCaller 동적 매핑

`UGE_HitReact_State` 의 `DurationMagnitude` 는 `SetByCaller(Data.Duration)`. 호출 측에서:
```cpp
Spec.Data->SetSetByCallerMagnitude(
    FGameplayTag::RequestGameplayTag("Data.Duration"),
    HitReactMontage->GetPlayLength());
```
이렇게 자산 길이에 맞춰 동적으로 duration 설정. BP 에서 HitReactMontage 를 교체하면 자동으로 적절한 차단 시간 유지.

#### 신규 게임플레이 태그 (DefaultGameplayTags.ini)

- `State.HitReact` — 피격 리액션 중 (공격/스킬 차단)
- `Data.Duration` — GE SetByCaller duration key

#### `UAOSAnimInstance` 의 `bIsHitReacting` 태그 미러

Phase 3.5 당시 TODO 로 남겨뒀던 부분 활성화. `NativeUpdateAnimation` 안에서
`ASC->HasMatchingGameplayTag("State.HitReact")` 로 갱신. ABP 가 이 변수로 transition 가능.

#### Hot Reload 비호환

신규 UCLASS `UGE_HitReact_State` + 신규 UFUNCTION `ApplyHitReactStateGE` 도입 → **풀 리빌드 필수**.

### Dedicated Server 네트워킹

- `PlayAnimMontage` 는 `ACharacter` 내장 replicate — 서버 호출 시 자동.
- `Multicast_PlayDeathMontage` / `Multicast_PlayHitReact` 는 명시 RPC — 서버 권한 진입점에서만 호출.
- AnimNotify 는 클라/서버 양쪽 실행 → notify 안에 게임플레이 로직 절대 금지 (현재는 빈 껍데기).
- 단일 진실 공급원: 쿨다운은 `Cooldown.Attack.Basic` 태그 (Phase 3 부터 적용) — 몽타주 길이 ≠ 쿨다운이어도 무영향.

### Death Montage + Root Motion + Ragdoll 전환

사망 시퀀스는 **(1) StateTree 정지 → (2) MovementMode 정정 → (3) Death montage 멀티캐스트 → (4) Ragdoll 전환 타이머 → (5) 액터 destroy** 5 단계.
각 단계마다 함정이 있었고 아래는 그 결과로 굳어진 패턴.

**`OnCharacterDeath` (서버) 핵심 호출 순서** — 순서 바꾸면 desync 또는 root motion 깨짐:

```cpp
void AAOSCharacter::OnCharacterDeath()
{
    if (!HasAuthority() || bIsDeadCached) return;
    bIsDeadCached = true;

    // (1) StateTree 즉시 정지 — 안 그러면 다음 tick 의 SendAttackEvent 가
    //     AttackMontage 를 재생해서 같은 DefaultSlot 의 DeathMontage 를 덮어씀.
    if (AController* C = GetController()) {
        if (UBrainComponent* Brain = Cast<AAOSAIController>(C) ?
            Cast<AAOSAIController>(C)->GetBrainComponent() : nullptr) {
            Brain->StopLogic(TEXT("Character died"));
        }
    }

    // (2) MovementMode 를 Walking 으로 강제 — AI 캐릭터의 기본 MOVE_NavWalking 은
    //     NavMesh 밖으로 나가는 root motion delta 를 snap-to-navmesh 로 무효화.
    if (UCharacterMovementComponent* CMC = GetCharacterMovement()) {
        if (CMC->MovementMode == MOVE_NavWalking) CMC->SetMovementMode(MOVE_Walking);
    }

    // (3) 사망 몽타주 멀티캐스트 — 서버+모든 클라가 각자 PlayAnimMontage + StartRagdoll 타이머.
    Multicast_PlayDeathMontage();

    // (4) GameMode 통보, lifespan 설정 등 (기존 로직)
    const float MontageLen = DeathMontage ? DeathMontage->GetPlayLength() : 0.f;
    SetLifeSpan(MontageLen + RagdollSettleDuration);
    // ⚠ SetActorTickEnabled(false) 호출 금지 — root motion 적용을 방해함.
    // ⚠ Capsule collision 변경 금지 — CMC::FindFloor sweep 실패 → MOVE_Falling 전환 → root motion 흐름 깨짐.
}
```

**`StartRagdoll` (각 머신 로컬, Multicast 의 타이머에서 호출)** — 이 시점부턴 root motion 끝났으므로 capsule 안전하게 끔:

```cpp
void AAOSCharacter::StartRagdoll()
{
    USkeletalMeshComponent* M = GetMesh();
    if (!M || !M->GetPhysicsAsset()) {                 // PhysicsAsset 없으면 fallback
        if (M) M->SetVisibility(false, true);
        return;
    }
    if (UCapsuleComponent* Cap = GetCapsuleComponent()) // 이제 안전 — root motion 종료됨
        Cap->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    if (UCharacterMovementComponent* CMC = GetCharacterMovement())
        CMC->SetMovementMode(MOVE_None);

    M->SetCollisionProfileName(TEXT("Ragdoll"));
    M->SetAllBodiesSimulatePhysics(true);
    M->WakeAllRigidBodies();
    M->bBlendPhysics = true;
}
```

**호출 시점 — `Multicast_PlayDeathMontage` 안에서**:
- DeathMontage + AnimInstance 둘 다 있으면: `PlayAnimMontage(DeathMontage)` + `MontageLength` 만큼 타이머 → `StartRagdoll`
- 둘 중 하나라도 없으면: 즉시 `StartRagdoll` (fallback)

**건드리면 안 되는 것 — 시도했다가 되돌린 변경**:
- ❌ `SetActorEnableCollision(false)` / `Capsule->SetCollisionEnabled(NoCollision)` 를 사망 진입 시점에 호출 — `CMC::FindFloor` sweep 실패 → `MOVE_Falling` 전환 → root motion 흐름 깨짐. (Ragdoll 진입 시점엔 OK)
- ❌ `Capsule->SetCollisionResponseToAllChannels(Overlap)` — 같은 이유.
- ❌ `SetActorTickEnabled(false)` — 정확한 메커니즘은 불명이지만 root motion 적용 안 됨.
- ❌ `AOSAIController::Tick` 의 사망 처리 블록에서 `StopMovement()` 호출 — `PathFollowingComponent::AbortMove` 가 CMC velocity 에 간섭하여 root motion 과 race. `Brain.StopLogic` 이 이미 StateTree 정지하므로 중복.

**시체 collision 정책**: 사망 후 액터 destroy 전까지 캡슐 collision 은 **유지** (Pawn vs Pawn 충돌 그대로). 시체가 살아있는 캐릭터를 막는 건 게임 특색으로 의도. Ragdoll 진입 후엔 캡슐 NoCollision (메시가 물리 시뮬되므로 캡슐은 더 이상 의미 없음).

### Root Motion 트러블슈팅 체크리스트

새 root motion 자산 도입 시 — 또는 root motion 이 추출은 되는데 액터가 안 움직일 때 — 다음 순서로 점검.

1. **AnimSequence 설정** — AS 의 디테일에서:
   - `Enable Root Motion = true`
   - `Force Root Lock = false` (true 면 root 가 origin 에 고정)
   - `Root Motion Root Lock = Anim First Frame` (또는 자산 의도에 맞게)
2. **AS preview viewport** — 자산 에디터에서 "Process Root Motion" 토글 ON → 캐릭터가 viewport 안에서 실제 이동하면 자산 데이터 정상. 안 움직이면 → 3번.
3. **C++ 진단** — `Montage->ExtractRootMotion(0, MontageLen, false)` 호출해서 결과 transform 의 translation 확인:
   - `HasRootMotion()=TRUE` 인데 `ExtractRootMotion(...)` 의 translation 이 (0,0,0) → **bone track 비어있음**, 데이터가 anim curve (`root_translation_Y` 등) 에만 있는 케이스 (Boss_Parried_RM 등 외부 자산에서 자주 발생).
   - 해결: AS 에디터 → `EncodeRootBoneModifier` 적용. source bone `pelvis`, axis `XYZ-Axes` → pelvis 의 track 데이터를 root 본 raw track 으로 baking. **자산 복제 후 적용 권장** (destructive 변경).
4. **AnimInstance RootMotionMode** — `UAnimInstance::RootMotionMode` 기본값이 `NoRootMotionExtraction` 이라 AnimSequence 의 `bEnableRootMotion=true` 도 무시됨. 우리는 `UAOSAnimInstance` 생성자에서:
   ```cpp
   RootMotionMode = ERootMotionMode::RootMotionFromMontagesOnly;
   ```
5. **런타임 로그** — `log LogRootMotion Verbose` 로 CMC 의 per-tick root motion 적용 추적. delta 가 매 tick 찍히면 추출은 정상.
6. **MovementMode 확인** — `showdebug character` 로 실시간 표시. `MOVE_NavWalking` 이면 NavMesh 밖으로 나가는 root motion 이 snap-to-navmesh 로 무효화 → `MOVE_Walking` 으로 전환 필요. (사망 시퀀스에선 이미 자동 처리, 다른 곳에서 root motion 쓰는 경우 주의)
7. **MovementMode 가 Falling 으로 바뀌었는지** — `FindFloor` sweep 이 실패하면 `MOVE_Falling` 으로 떨어지면서 root motion translation 이 중력/공기저항 로직에 가려짐. 캡슐 collision 을 끄거나 floor 가 갑자기 사라진 경우 발생.

### Dedicated Server 환경에서의 Root Motion

DS 에서 montage-driven root motion 은 다음 흐름으로 동작:

```
[서버] PlayAnimMontage (예: Multicast_PlayDeathMontage 의 ImplementationAll branch)
   ↓ AnimInstance tick → RootMotionFromMontagesOnly → ExtractRootMotion
   ↓ CMC::PerformMovement 가 RootMotionDelta 를 액터 위치에 적용 (Authority)
   ↓ ACharacter 가 root motion source 를 RepRootMotion 으로 직렬화
   ↓
[리플리케이션]
   ↓
[각 클라이언트] OnRep_RootMotion → ACharacter::SimulatedRootMotionPositionFixup
   ↓ CanUseRootMotionRepMove / FindRootMotionRepMove 로 서버와 시간/위치 동기화
   ↓ AnimInstance 가 같은 montage 를 같은 위치에서 재생 (Multicast 가 이미 PlayAnimMontage 호출)
```

**중요한 함정**:
- **서버에서 StateTree 가 살아있으면 죽음 직후 SendAttackEvent → AttackMontage 가 DeathMontage 를 덮어씀** → 서버는 Attack 재생, 클라는 Death 재생하는 **desync 발생**. 반드시 `OnCharacterDeath` 의 (1) 단계 `Brain.StopLogic` 으로 정지.
- `Multicast_PlayDeathMontage` 는 서버에서도 실행됨 (NetMulticast 의 표준 동작) → 서버 자신도 PlayAnimMontage 호출하므로 서버 권한 액터의 위치가 root motion 으로 실제 이동.
- 클라이언트는 `OnRep_RootMotion` 이 도착하기 전까지는 서버 위치를 따라가지만, RootMotion replication 이 시작되면 자체 시뮬레이션으로 전환 → 짧은 보간 갭이 생길 수 있음 (현재는 무시 가능 수준).

### 자산 폴더 구조 (예정 — 사용자 import 후)

```
Content/AOS/Anim/
├── ABP_AOSCharacter.uasset           // parent = UAOSAnimInstance
├── BS_Locomotion.uasset              // BlendSpace1D, axis=Speed 0..600
├── Montages/
│   ├── AM_Attack.uasset              // slot=DefaultSlot, AnimNotify_AttackHit @ ~0.4s
│   ├── AM_HitReact.uasset            // slot=UpperBody
│   ├── AM_Death.uasset               // slot=FullBody
│   └── AM_Skill_<Name>.uasset        // Phase 4
└── Notifies/                         // 비움 (C++ notify 만 사용)
```

`SetupCharacterDefaults` 가 `/Game/AOS/Anim/ABP_AOSCharacter` 를 우선 로드, 자산 없으면 `ABP_Unarmed` (UE5 기본) 폴백.

### Skeletal Mesh

현재 모든 캐릭터는 **UE5 Mannequin (`SKM_Manny_Simple` + `IK_Mannequin`)** 을 dev 타깃으로.
사용자가 다른 메시/스켈레톤 자산을 가져오면 IK Retargeter 로 SK_Mannequin 호환으로 retarget 후 같은 슬롯에 꽂으면 됨.
캐릭터별 derived BP (`BP_Warrior` 등) 도입 시점에 캐릭터별 ABP / Skill 몽타주 매핑을 분리한다.

### 사용자 수동 작업 (자산 import 후)

1. **자산 import**: UE5 Mannequin 호환 또는 IK Retargeter 로 retarget. `Content/AOS/Anim/Montages/` 에 배치.
2. **몽타주 슬롯**: AM 자산의 Anim Slot Manager 에 `DefaultSlot` (Attack/Death) / `UpperBody` (HitReact) 그룹 설정.
3. **AttackHit Notify**: `AM_Attack` 타임라인에 `AOSAnimNotify_AttackHit` 트랙 추가, 무기 충격 프레임 (보통 0.3~0.5s).
4. **BP_Character 디테일**:
   - Mesh → Skeletal Mesh / Anim Class
   - AOS|Animation → 4 개 Montage 슬롯 + SkillMontages map (Phase 4)

### Hot Reload 비호환

신규 UCLASS (`UAOSAnimInstance`, `UAOSAnimNotify_AttackHit`) 추가 → **풀 리빌드 필수**.
신규 UPROPERTY (`RagdollSettleDuration`) / UFUNCTION (`StartRagdoll`, `Multicast_PlayDeathMontage` 시그니처 변경) 도입 시점에도 풀 리빌드 — 현재는 적용 완료, history 기록용.
이후 슬롯 채우기 / 몽타주 변경은 hot reload OK.

## Memory Management Patterns

### UPROPERTY Requirements

**Critical rule**: All `UObject*` pointer arrays MUST be marked with `UPROPERTY()` for garbage collection.

```cpp
// CORRECT
UPROPERTY()
TArray<AAOSStructure*> AllTowers;

// WRONG - Will cause crashes
TArray<AAOSStructure*> AllTowers;
```

### AI Controller Cleanup

AI controllers must explicitly clean up in destructor:

```cpp
AAOSAIController::~AAOSAIController()
{
    WaypointQueue.Empty();
    ControlledCharacter = nullptr;
    CurrentTarget = nullptr;
}
```

**Why**: Prevents dangling pointers when editor closes or PIE ends.

## Collision System

Structures (towers/command centers) use specific collision settings:

- **CollisionComponent**: `ECollisionEnabled::NoCollision` - No physical blocking
- **MeshComponent**: `ECollisionEnabled::NoCollision` - Visual mesh doesn't block
- **DetectionRange**: `ECollisionEnabled::QueryOnly` - Overlap detection for finding enemies

**Rationale**: Characters should pass through towers, not be blocked by them.

## Debug Visualization

### Runtime Debug (Play Mode)

`AOSMapManager::DrawDebugTowerPositions()` draws persistent debug boxes:
- Blue boxes = Team1 towers
- Red boxes = Team2 towers
- Yellow box = Team1 Command Center (1.5x size)
- Orange box = Team2 Command Center (1.5x size)

**Important**: This function iterates `AllTowers` (actual spawned actors), NOT `LanesInfo` (configuration).

### Editor Visualization (Edit Mode)

`UpdateEditorVisualization()` uses `DrawDebugLine` to show lane paths and tower positions:
- Uses `LanesInfo` configuration data
- Draws X markers for towers, wireframe boxes for command centers
- Note: `DrawDebugBox` doesn't render properly in editor viewport, use `DrawDebugLine` instead

## Common Development Patterns

### Adding New Structure Types

1. Extend `AOSStructure` base class
2. Set `StructureType` enum value
3. Add to spawning logic in `AOSMapManager::SpawnStructures()`
4. Update waypoint queue building logic in `AOSAIController::BuildWaypointQueue()`

### Modifying AI Behavior

Key parameters in `AOSAIController`:
- `EnemyDetectionRange` (default: 1500.0f) - How far AI can see enemy characters
- `AttackRange` (default: 500.0f) - Distance to begin attacking enemy characters
- `ArrivalDistance` (100.0f) - How close to waypoint before considering "arrived"
- `AttackCooldownDuration` (1.0f) - Cooldown between attacks

AI behavior loop in `UpdateAIBehavior()`:
1. Check for nearby enemy characters (`FindNearestEnemy()`)
2. If enemy character found → `AttackTarget()` (move + attack)
3. If no enemy character → Check current waypoint:
   - If waypoint is an enemy structure → `AttackStructure()` (move + attack structure)
   - If no structure target → `MoveTowardsTarget()` (move toward next waypoint)

### HP Bar Setup (Widget Blueprint)

After building, create `WBP_HealthBar` in editor:
1. Content Browser → Content/AOS/UI/ → Right-click → User Interface → Widget Blueprint
2. Parent class: `AOSHealthBarWidget`
3. Add ProgressBar, rename it to exactly `HealthProgressBar`
4. Open BP_Character → HealthBarComponent → Widget Class → select `WBP_HealthBar`
5. Same for any Structure Blueprints that use `HealthBarWidgetClass`

### Testing in PIE (Play In Editor)

Quickest test setup:
1. Place `AOSMapManager` in level
2. Configure `LanesInfo` with 3 lanes (Top, Mid, Bottom)
3. Set tower positions and command center positions
4. Place 12 `AOSSpawnPoint` actors with correct Team/Lane/Index
5. Play (PIE) - Characters auto-spawn and begin pushing

See `Guides/04_UsageGuide/QUICK_TOWER_TEST.md` for detailed checklist.

## Project Structure

```
TDProject/
├── Source/TDProject/
│   ├── AOS/                           # Main game system (Active)
│   │   ├── AOSGameMode.*              # Game state management
│   │   ├── AOSCharacter.*             # Character base class
│   │   ├── AOSAIController.*          # AI movement & combat
│   │   ├── AOSPlayerController.*      # Player input (minimal)
│   │   ├── AOSStructure.*             # Towers & Command Centers
│   │   ├── AOSMapManager.*            # Map layout & spawning
│   │   ├── AOSSpawnPoint.*            # Character spawning
│   │   └── UI/
│   │       └── AOSHealthBarWidget.*   # HP bar 3D world widget
│   ├── Variant_Combat/                # Secondary system (Unused)
│   └── TDProject.*                    # Default UE starter files
├── Content/
│   └── AOS/UI/                        # Widget Blueprint assets (WBP_HealthBar)
├── Guides/                            # Extensive documentation (Korean)
│   ├── 01_GameOverview/               # Architecture docs
│   ├── 02_ProgressLog/                # Development history
│   ├── 03_ClassReview/                # Implementation details
│   └── 04_UsageGuide/                 # Setup & testing guides
└── TDProject.uproject                 # UE 5.7 project file
```

**Note**: The `Variant_Combat` folder contains an older combat system prototype and is not currently used.

## Documentation

The `Guides/` folder contains comprehensive Korean-language documentation:

- **`DOCUMENTATION_INDEX.md`** - Master index of all docs
- **`01_GameOverview/AOS_SYSTEM_OVERVIEW.md`** - Full system architecture
- **`02_ProgressLog/2026-01-05_MAP_STRUCTURE_IMPROVEMENTS.md`** - Recent work log (waypoint system, memory fixes, debug improvements)
- **`04_Implementation/WAYPOINT_QUEUE_SYSTEM.md`** - Detailed waypoint queue explanation

**Read these first** when making significant changes to understand design intent.

## Git Workflow

Recent commits show the pattern:
```bash
git add [modified files]
git commit -m "커밋 제목

상세 설명
...

🤖 Generated with [Claude Code](https://claude.com/claude-code)

Co-Authored-By: Claude Sonnet 4.5 <noreply@anthropic.com>"
git push
```

**Commit message format**: Korean title with detailed Korean description, Claude attribution footer.

## Known Issues and Gotchas

### Editor Crash on Exit

**Fixed**: Added destructors to AI controllers to clean up waypoint queues and null out pointers.

### Towers Not Visible

**Cause**: `DrawDebugTowerPositions()` was using `LanesInfo` instead of `AllTowers`
**Solution**: Always use `AllTowers` array for runtime visualization

### Characters Go to Map Center

**Cause**: Waypoint queue not built or built incorrectly
**Check**: Verify `BuildWaypointQueue()` is called in `StartDeployment()`

### AI Not Moving

**Causes**:
1. MapManager not placed in level
2. Spawn points not configured with correct Team/Lane
3. AI controller not possessing character
4. Waypoint queue empty

**Debug**: Check log output from `BuildWaypointQueue()` showing waypoint list

### HP Bar Not Showing

**Cause**: Widget Blueprint (`WBP_HealthBar`) not created or not assigned
**Solution**:
1. Create Widget Blueprint with parent class `AOSHealthBarWidget`
2. Add ProgressBar named exactly `HealthProgressBar`
3. Assign `WBP_HealthBar` to BP_Character's `HealthBarComponent` → Widget Class

### Surviving Characters Disappear After Winning Fight

**Cause**: Characters couldn't attack structures — they only moved toward them while towers attacked back
**Fixed**: Added `AttackStructure()` to `AOSAIController`. Characters now attack structures when the waypoint is an enemy structure.

### 라인 시작 위치 이중화 — SpawnPoint 가 단일 진실 공급원

**Cause**: 과거에는 `FLaneInfo::Team1StartPosition` / `Team2StartPosition` 와 `AAOSSpawnPoint`
액터가 별도로 라인 시작 위치를 표현했음. 둘이 동기화 깨질 위험(MCP 자동화로 한쪽만 변경되거나
저장 누락 등) 이 실제로 발생.

**Fixed**: `FLaneInfo` 에서 두 필드 제거. 이후 라인 시작은 `(Team, Lane)` 매칭되는
`AAOSSpawnPoint::GetActorLocation()`이 유일한 진실 공급원.
- `AAOSMapManager::GetLaneStartPosition()` — 내부에서 `GameMode->GetNearestSpawnPoint()` 사용
- `AAOSAIController::CacheLaneInfo()` — 캐릭터 위치를 직접 캐시 (스폰 직후라 SpawnPoint 위치와 동일)
- 에디터 시각화는 `ResolveLaneStartForVisualization()` 헬퍼가 SpawnPoint 매칭 → 첫 타워 fallback

### 지형/필드 변경 시 RecastNavMesh 반드시 재빌드

**증상**: 지형 액터(StaticMeshActor, Floor, Tower 등) 의 위치/스케일을 바꾸거나
`NavMeshBoundsVolume` 영역을 변경한 뒤 PIE 를 돌리면 캐릭터가 정해진 곳에서 벗어나지
못하거나(이전 nav 영역 밖) 아예 가만히 서 있음.

**Cause**: 프로젝트의 `[/Script/NavigationSystem.RecastNavMesh]` 설정이 `RuntimeGeneration=Static`
(default). 즉 `RecastNavMesh` 데이터는 **에디터에서 명시적으로 빌드된 시점의 cooked 데이터**
이고 런타임에 자동 재생성되지 않음. 지형이 바뀌었는데 NavMesh 가 옛 영역으로 cooked
돼 있으면 AI 의 `MoveTo` / pathfinding 이 실패. `ServerTravel(MainMenu→ThirdPerson)` 같은
레벨 전환에서도 stale nav 가 그대로 로드됨.

**대응 — 지형/Nav 영역 변경 후 반드시**:
1. 에디터 메뉴: **Build → Build Paths Only** (또는 `Ctrl+Shift+B`)
2. 변경된 레벨 + Nav 관련 액터들 (`RecastNavMesh`, `NavMeshBoundsVolume`) **저장**
3. 커밋에 nav uasset (`Content/__ExternalActors__/.../RecastNavMesh*`, `NavMeshBoundsVolume*`)
   포함되어 있는지 확인

특히 다음 작업 후엔 잊지 말 것:
- `StaticMeshActor` 위치/스케일 일괄 변경 (지형 ×N 확장 등)
- 새 지형 액터 추가 / 삭제
- `NavMeshBoundsVolume` 의 위치/스케일 조정
- 타워·CC 위치 재배치

**근본 fix 옵션 (선택)**: `Project Settings → Navigation Mesh → Runtime Generation`
을 `Dynamic` 으로 바꾸면 런타임에 nav 가 자동 빌드됨. 단 cook 비용·빌드 시간 증가.
현재는 명시적 rebuild 정책 유지.

## Multi-Agent Development Workflow (3도메인)

이 프로젝트는 **3개 도메인** 멀티 에이전트 방식으로 개발됩니다. 각 에이전트는 `.claude/agents/<name>.md` 의 **서브에이전트**로 정의되어 있고, `Task` 도구로 `subagent_type` 을 지정해 호출합니다. 오케스트레이션용 슬래시 커맨드 `/multi-agent`, `/merge-agents` 만 `.claude/commands/` 에 남아 있습니다.

> 서브에이전트는 별도 컨텍스트와 모델로 동작합니다. 슬래시 커맨드는 부모 세션의 모델을 그대로 쓰며 모델 분리가 안 되므로 사용하지 않습니다.

### 도메인 1: 프로그래머 (C++ 코드, model: sonnet)

작업 방식: git worktree + C++ 파일 편집

| 서브에이전트 | 모델 | 소유 파일 | 충돌 위험 |
|----------|------|-----------|-----------|
| `agent-prog-ai` | sonnet | AOSAIController.h/cpp | HIGH |
| `agent-prog-character` | sonnet | AOSCharacter.h/cpp, AOSSpawnPoint.h/cpp, AOSGameMode.h/cpp | HIGH (enum 소유) |
| `agent-prog-object` | sonnet | AOSStructure.h/cpp, AOSMapManager.h/cpp | MEDIUM |
| `agent-prog-ui` | sonnet | AOSHealthBarWidget.h/cpp, AOSPlayerController.h/cpp | LOW |
| `agent-prog-anim` | sonnet | AOSAnimInstance.h/cpp, Anim/AOSAnimNotify_*.h/cpp | MEDIUM |
| `agent-build-verify` | sonnet | 없음 (읽기 전용) | NONE |

### 도메인 2: 기획자 (밸런스, 레벨, 문서, model: opus)

작업 방식: MCP 도구 (worktree 불필요, 에디터에서 직접 수정)

| 서브에이전트 | 모델 | 소유 영역 |
|----------|------|-----------|
| `agent-design-balance` | opus | Blueprint EditAnywhere 파라미터 전체 |
| `agent-design-level` | opus | 레벨 액터 배치, MapManager 설정 |
| `agent-design-docs` | opus | CLAUDE.md, Guides/ 전체 |

### 도메인 3: 아트 (비주얼, VFX, model: haiku)

작업 방식: MCP 도구 (worktree 불필요, 에디터에서 직접 수정)

| 서브에이전트 | 모델 | 소유 에셋 |
|----------|------|-----------|
| `agent-art-visual` | haiku | 머티리얼, 텍스처, 메시, 팀 색상 |
| `agent-art-vfx` | haiku | Niagara VFX, UI 스타일링 |
| `agent-art-anim` | haiku | 애니메이션 BP, 몽타주, 블렌드 스페이스 |

### Workflow

1. `/multi-agent [기능 설명]` — 오케스트레이터가 도메인 분류 + 에이전트 할당
2. **프로그래머**: worktree 생성 (브랜치: `agent/prog-<role>/<feature>`), 별도 터미널/세션에서 `agent-prog-*` 서브에이전트 호출 (병렬)
3. **기획자/아트**: 오케스트레이터가 같은 세션에서 `Task(subagent_type: "agent-design-*" / "agent-art-*", ...)` 로 직접 spawn
4. `/merge-agents` — 프로그래머 머지 → 기획 검증 → 아트 검증

### 머지/검증 순서 (3단계)

**Phase 1: 프로그래머 머지** (순차)
1. `agent-design-docs` (코드 충돌 없음)
2. `agent-prog-ui` (최소 외부 의존성)
3. `agent-prog-anim` (AnimInstance, Character 의존)
4. `agent-prog-object` (중간 결합도)
5. `agent-prog-character` (enum 소유, API 제공)
6. `agent-prog-ai` (최고 결합도, 마지막)
→ 각 단계 후 `agent-build-verify` 실행

**Phase 2: 기획자 검증** (MCP, 머지 불필요)
- `agent-design-balance`: `get_property` 로 값 확인
- `agent-design-level`: `get_level_actors` 로 배치 확인

**Phase 3: 아트 검증** (MCP, 머지 불필요)
- `agent-art-visual`: 머티리얼 적용 확인
- `agent-art-vfx`: VFX 확인
- `agent-art-anim`: 애니메이션 확인
→ `capture_viewport` 로 시각 검증

### AOSGameMode.h Enum 변경 게이트

`EAOSTeam`, `EAOSLane`, `EAOSGameState` enum은 모든 AOS 파일이 의존합니다.
enum 변경이 필요하면 **반드시 main에 먼저 커밋한 후** 에이전트 브랜치를 생성하세요.

### 도메인 간 조율

하드코딩 값 발견, 새 컴포넌트 슬롯 필요 등 도메인 간 요청은 `CROSS_DOMAIN_REQUESTS.md`에 등록합니다.

### 조율 파일

- `.claude/coordination/INTERFACE_CONTRACTS.md` — 에이전트 간 안정 인터페이스 + Blueprint 프로퍼티 계약
- `.claude/coordination/AGENT_STATUS.md` — 활성 에이전트 세션 추적 (worktree/MCP 구분)
- `.claude/coordination/CROSS_DOMAIN_REQUESTS.md` — 도메인 간 변경 요청 추적
- `.claude/coordination/ASSET_OWNERSHIP.md` — Content/ 에셋 소유권 매핑

## Dedicated Server 환경

이 프로젝트는 **Dedicated Server (DS)** 로 실행됩니다. 모든 에이전트는 아래 사실을 전제로 작업해야 합니다.

### 실행 위치 표

| 시스템 | DS (서버) | 각 클라이언트 |
|--------|:--------:|:------------:|
| **GameMode** (`AAOSGameMode`) | ✅ 유일 인스턴스 | ❌ `GetAuthGameMode()` = null |
| **GameState** (`AAOSGameState`) | ✅ 권한 (Authority) | ✅ 리플리케이션된 읽기 전용 복제본 |
| **AIController** (`AAOSAIController`) | ✅ 모든 AI 인스턴스 | ❌ 없음 |
| **PlayerController** (`AAOSPlayerController`) | ✅ 각 접속자의 서버사이드 PC | ✅ 로컬 PC (LocalPlayerController) |
| **Character/Structure** | ✅ 스폰/파괴 권한 | ✅ 리플리케이션된 복제본 |
| **UI 위젯·카메라** | ❌ 생성 금지 | ✅ `IsLocalPlayerController()` 블록 내에서만 |
| **VFX/사운드/머티리얼 렌더** | ❌ 렌더 파이프라인 없음 | ✅ 시각/청각 출력 |

### 핵심 규칙

1. **`HasAuthority()` 가드** — 상태 변경(HP, 스폰, 파괴, 라운드 전환 등)은 반드시 서버에서만 실행
2. **`IsLocalPlayerController()` 가드** — 모든 `CreateWidget` / `AddToViewport` / 카메라 생성 앞에 필수
3. **`GetAuthGameMode()` 사용 금지 (클라이언트 로직)** — 클라이언트는 null을 받으므로 **GameState** 경유
4. **디버그 출력**: `GEngine->AddOnScreenDebugMessage()`, `DrawDebugLine()` 등은 DS에서 렌더 없음 → NetMode 체크 또는 클라이언트 RPC 경유
5. **리플리케이션 패턴**: `UPROPERTY(ReplicatedUsing = OnRep_*)` + `DOREPLIFETIME(...)` + `OnRep_*()` 콜백 세트
6. **클라 → 서버 요청**: `UFUNCTION(Server, Reliable)` + `Server_*_Implementation`
7. **서버 → 모든 클라 방송**: `UFUNCTION(NetMulticast, Reliable)` 또는 Replicated 프로퍼티 + OnRep

### 클라이언트 UI가 서버 상태를 읽는 흐름

```
[서버] GameMode 상태 변경
   ↓
[서버] GameState::ServerSet*() → Replicated 프로퍼티 갱신
   ↓
[네트워크 리플리케이션]
   ↓
[클라이언트] GameState::OnRep_*() 콜백
   ↓
[클라이언트] Dynamic Multicast Delegate 브로드캐스트
   ↓
[클라이언트] PlayerController → Widget::UpdateXxx()
```

**안티패턴**: 클라이언트 코드에서 `GetWorld()->GetAuthGameMode<AAOSGameMode>()->GetCurrentRound()` — 클라이언트에서 null crash.
**올바른 패턴**: `GetWorld()->GetGameState<AAOSGameState>()->GetCurrentRound()`

### PIE 테스트

- 반드시 **Play As Dedicated Server** 또는 Listen Server 2-Client 모드로 테스트
- 단일 프로세스(Single Process)는 DS 환경을 정확히 재현하지 못함

## Language Notes

- **User messages**: Often in English asking for Korean translations
- **Code comments**: Mix of English and Korean
- **Documentation**: Primarily Korean
- **Logs**: Mix of English and Korean
- **When working with this user**: Provide Korean translations of technical explanations when requested
