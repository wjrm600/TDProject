# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

**TDProject** is an Unreal Engine 5.7 MOBA-style (AOS - Auto Oriented Strategy) game with 3-lane tower defense mechanics. The project implements AI-controlled characters that push lanes, attack towers sequentially, and compete to destroy the enemy Command Center.

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
