# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

**TDProject** is an Unreal Engine 5.7 MOBA-style (AOS - Auto Oriented Strategy) game with 3-lane tower defense mechanics. The project implements AI-controlled characters that push lanes, attack towers sequentially, and compete to destroy the enemy Command Center.

## Build Commands

### Building the Project

```bash
# Build with Unreal Engine 5.7 (using PowerShell)
powershell -Command "& 'C:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\Build.bat' TDProject Win64 Development -Project='C:\UnrealProject\TDProject\TDProject.uproject'"
```

**Note**: This project uses Visual Studio 2026 and Unreal Build Accelerator (UBA).

### Opening in Editor

Open `TDProject.uproject` in Unreal Editor 5.7. The project will compile automatically if needed.

### Hot Reload

Use the Unreal Editor's hot reload feature (Ctrl+Alt+F11) for quick C++ changes without full rebuild.

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
- `LanesInfo` = Editor configuration (what you set in the Blueprint)
- `AllTowers` = Runtime spawned instances (actual actors in the world)
- Always use `AllTowers` for runtime logic, `LanesInfo` for spawning

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

**AOSCharacter** - Player-controlled character base class
- Health, attack, movement properties
- Replicates for multiplayer (though networking not fully implemented)

**AOSAIController** - AI behavior and movement
- **Most complex class in the project**
- Implements waypoint queue system
- Handles enemy detection, combat, and lane pushing
- Uses `Tick()` for behavior updates (not Behavior Trees or State Trees)

**AOSStructure** - Base class for towers and command centers
- Health management
- Auto-attack system for defensive structures
- Detection range for finding enemies

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
- `EnemyDetectionRange` (default: 1500.0f) - How far AI can see enemies
- `AttackRange` (default: 500.0f) - Distance to begin attacking
- `ArrivalDistance` - How close to waypoint before considering "arrived"

AI behavior loop in `UpdateAIBehavior()`:
1. Check for nearby enemies (`FindNearestEnemy()`)
2. If enemy found → Attack (`AttackTarget()`)
3. If no enemy → Continue waypoint movement (`MoveTowardsTarget()`)

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
│   │   └── AOSSpawnPoint.*            # Character spawning
│   ├── Variant_Combat/                # Secondary system (Unused)
│   └── TDProject.*                    # Default UE starter files
├── Content/                           # Blueprint assets
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

## Language Notes

- **User messages**: Often in English asking for Korean translations
- **Code comments**: Mix of English and Korean
- **Documentation**: Primarily Korean
- **Logs**: Mix of English and Korean
- **When working with this user**: Provide Korean translations of technical explanations when requested
