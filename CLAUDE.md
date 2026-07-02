# CLAUDE.md

Claude Code 가 이 저장소에서 작업할 때의 지침입니다. **이 파일은 매 작업마다 적용되는 "필수 규칙 + 핵심 아키텍처 + live 함정"의 슬림 버전**입니다.

> 📚 **깊은 상세는 [`Guides/01_GameOverview/PROJECT_REFERENCE.md`](Guides/01_GameOverview/PROJECT_REFERENCE.md)** — GAS Phase별 마이그레이션 기록, StateTree task/condition 전체 표, 애니메이션 시스템 상세, 벤픽/상점 UI 저작 계약, 경제 시스템 상세, 해결된 historical 이슈가 모두 거기 있습니다. **GAS/StateTree/애니/벤픽 내부를 수정하기 전엔 PROJECT_REFERENCE 의 해당 섹션을 먼저 읽으세요.**
> 둘이 어긋나면 CLAUDE.md 가 우선이고, 변경 시 양쪽을 맞추세요.

## Project Overview

**TDProject** — Unreal Engine 5.7 기반 **오토배틀러 MOBA (AOS)**. 3레인 타워디펜스. AI가 조종하는 캐릭터들이 라인을 밀고 타워를 순차 공격해 적 Command Center 파괴를 겨룬다. 플레이어 입력은 **벤픽 드래프트 + 라운드 사이 상점/배치**, 전투는 관전. 북극성 = Mechabellum. 비전 문서: [`Guides/01_GameOverview/GAME_VISION.md`](Guides/01_GameOverview/GAME_VISION.md).

## ⚠️ 필수 운영 규칙 (매 작업)

- **빌드 금지(에이전트)**: 직접 `Build.bat`/풀 빌드 실행하지 말 것. 핫 리로드(Ctrl+Alt+F11) 제안 또는 사용자에게 빌드/검증 요청. (신규 USTRUCT/UCLASS/UPROPERTY/모듈 의존성 변경은 **풀 리빌드 필수** — 핫 리로드 비호환.)
- **main 직접 작업**: worktree 가 아닌 main 프로젝트 파일을 편집 (사용자가 main 에서 테스트). 커밋/푸시는 사용자가 요청할 때만.
- **Dedicated Server 전제**: 모든 상태 변경은 `HasAuthority()`, 모든 위젯/카메라 생성은 `IsLocalPlayerController()` 가드. 아래 [Dedicated Server 환경](#dedicated-server-환경-필수) 참고 — 이 규칙들은 매 작업 적용된다.
- **작업 타임라인 갱신**: 의미 있는 작업(feat/fix/refactor/트러블슈팅 원인확정/새 시스템 도입/외부 비호환 발견) 완료 시 [`Guides/05_ProgressLog/TIMELINE.md`](Guides/05_ProgressLog/TIMELINE.md) 에 항목 추가. 형식 = `작업 내용` / `문제점` / `해결 방법` / `결과(+커밋 해시)`. 시간 순(오래된 것 위), 의미 단위로 묶어 1항목. **사용자가 스크린샷 공유 시** → 먼저 `Guides/05_ProgressLog/images/<YYYY-MM-DD_주제>/` 폴더 생성 후 "여기 넣어달라" 요청 → 타임라인에 상대경로 임베드. (상세 규칙 → PROJECT_REFERENCE)
- **MCP 에셋 편집 함정** (must-follow):
  - DataTable 행 추가/수정 = `export_data_table_to_json_string` ↔ `fill_data_table_from_json_string` **JSON 라운드트립**(기존 필드 보존).
  - `EditDefaultsOnly` 구조체 = `set_editor_property` 가 "cannot be edited on instances" 로 막힘 → **`struct.import_text("(Field=Value,...)")`** 우회.
  - BP CDO 편집 후 **`save_asset(path, only_if_is_dirty=False)` 강제 저장 필수** (안 하면 디스크 미반영 → 재시작 시 유실). 구조체 필드는 snake_case.

## Build & MCP

```powershell
# 엔진 경로는 머신마다 다름 → 환경변수 $env:UE_ROOT 사용 (등록: Mcp_Tools/README.md §1-1)
& "$env:UE_ROOT\Engine\Build\BatchFiles\Build.bat" `
    TDProject Win64 Development -Project="<PROJECT_ROOT>\TDProject.uproject"
```
- VS 2026 + Unreal Build Accelerator(UBA). 에디터에서 `TDProject.uproject` 열면 자동 컴파일.
- **MCP 2서버**: `unreal-engine`(에디터 자동화 — 에디터가 WebSocket `:8091` → `unreal-engine-mcp-server`(npx) 중계) + `unreal-rag`(C++ 코드 RAG, `Mcp_Tools/ue_rag_mcp.py`). 새 머신 셋업 = [`Mcp_Tools/README.md`](Mcp_Tools/README.md).

## 핵심 아키텍처 (불변)

### Waypoint Queue System (가장 중요)
AI 이동은 **pathfinding 이 아니라 웨이포인트 큐** 패턴. 각 AI가 방문할 구조물 큐를 만들어 순서대로 진행:
아군 타워(스폰 가까운 순) → 적 타워(스폰 가까운 순) → 적 CC. 파괴된 웨이포인트는 자동 skip, 적 감지 시 전투가 이동을 인터럽트.
- `AOSAIController::BuildWaypointQueue()` / `GetNextTargetLocation()` / `MoveTowardsTarget()`
- ⚠️ **이 시스템을 모른 채 navmesh/pathfinding 기능 추가로 "고치려" 하지 말 것.** 큐가 MOBA 식 라인 푸시를 보장한다. 상세: [`Guides/04_Implementation/WAYPOINT_QUEUE_SYSTEM.md`](Guides/04_Implementation/WAYPOINT_QUEUE_SYSTEM.md).

### Map / Lane System
- 3레인(Top/Mid/Bottom), 팀당 타워 9(레인당 3) + CC 1. `AOSMapManager` 가 `BeginPlay` 에서 스폰.
- **`LanesInfo`(에디터 설정, 타워 좌표만) ≠ `AllTowers`(런타임 스폰 인스턴스)** — 런타임 로직은 항상 `AllTowers`, 스폰은 `LanesInfo`.
- **라인 시작 = SpawnPoint 가 단일 진실 공급원**. `FLaneInfo` 는 `Team*StartPosition` 없음(제거됨). `(Team,Lane)` 매칭 `AAOSSpawnPoint::GetActorLocation()` 이 시작 위치. `GetLaneStartPosition()` 내부는 `GetNearestSpawnPoint()` 경유(클라는 ZeroVector).

### Enums (AOSGameMode.h 소유)
`EAOSTeam{Team1,Team2}`, `EAOSLane{Top,Mid,Bottom}`, `EAOSGameState{...,BanPick=5}`(enum **끝에 append** — 중간 삽입 금지). ⚠️ **enum 변경은 모든 AOS 파일이 의존 → 반드시 main 에 먼저 커밋 후 에이전트 브랜치 생성.**

### Core Classes
| 클래스 | 역할 |
|--------|------|
| `AOSGameMode` (서버 전용) | 게임 상태 전이, 승패, 골드 적립, 벤픽 드래프트 권한, 라운드 스폰 |
| `AOSGameState` | 클라 리플리케이션 (상태/라운드/골드/드래프트/라운드결과) — **클라는 GameMode 없으니 GameState 경유** |
| `AOSMapManager` | 맵 레이아웃 + 구조물 스폰(`HasAuthority` 가드) |
| `AOSSpawnPoint` | 캐릭터 스폰 (12개: 2팀×3레인×2) |
| `AOSCharacter` | AI 캐릭터 베이스. ASC+AttributeSet 소유. IAbilitySystemInterface |
| `AOSAIController` | **가장 복잡** — 웨이포인트 큐 + StateTree 부착. 헬퍼는 ST task 용 public |
| `AOSStructure` | 타워/CC 베이스. ASC 통합(Phase 5). Tower HP 1000 / CC 5000 |
| `AOSPlayerController` | 입력/카메라/위젯(로컬 PC), 서버 RPC 진입점 |
| `AOSHealthBarWidget` | 3D 월드 HP바. `WBP_HealthBar`(에디터) 필요, `HealthProgressBar` BindWidget |

### Character Lifecycle
SpawnPoint `SpawnCharacter` → `InitializeCharacter`(팀/레인) → AIController possess → `DeployToLane` → `StartDeployment`(여기서 StateTree `StartLogic()` 수동 호출 + 웨이포인트 빌드) → StateTree tick → 사망 시 `OnCharacterDeath`(아래 애니 섹션 순서 준수) → 2s 후 destroy(리스폰 없음).

## AI: State Tree (현재 아키텍처)

AI 행동 결정은 **State Tree** (`Source/TDProject/AOS/AI/`). 이전 `UpdateAIBehavior` if/else 제거됨.
- `UStateTreeAIComponent` 가 `AAOSAIController` 에 부착. **`bStartLogicAutomatically=false`** + `StartDeployment()` 에서 수동 `StartLogic()` (팀 확정 후 시작 — 안 그러면 아군 오사).
- 자산 `/Game/AOS/AI/ST_AOSCharacterAI`. **스키마=StateTreeAIComponentSchema, ContextActorClass=`AOSCharacter`(Pawn, AIController 아님!).**
- **Design B (재선택 패턴)**: 우선순위 = 자식 노드 순서 한 곳(`UseR→UseW→UseQ→UseE→AttackEnemy→AttackStructure→PushLane`, PushLane 은 조건 없는 fallback 이라 **맨 마지막**). RUNNING 유지 state(PushLane/AttackEnemy)만 `On Tick → Root` 재선택 트랜지션. 스킬 task 는 즉시 완료형이라 트랜지션 불필요.

**자주 빠지는 함정** (대부분 여기서 해결):
1. ContextActorClass 가 AIController 로 잘못 설정 → `AOSCharacter` 로.
2. `bStartLogicAutomatically=true` → BeginPlay 시 `GetPawn()=null` schema 실패. false + StartDeployment 에서 수동 시작.
3. Running task state 는 자동 재선택 안 됨 → 적 만나도 안 싸움. Root 의 `On Tick` 트랜지션으로 강제.
4. InstanceData 의 `AIController` 는 **base `TObjectPtr<AAIController>`** 로 선언(derived 로 하면 schema binding 실패), cpp 에서 `Cast<AAOSAIController>`.
5. `HasCooldownTag` 는 `bInvert=true` ("쿨다운 **없을 때** 사용 가능").
6. StartLogic 을 OnPossess 에서 호출 → SetTeam 전 평가 → Team2 가 아군 오사. StartDeployment 로 이동.

> Task/Condition 전체 표 + ST 자산 작성 단계 + 디버깅 명령(`showdebug ai`, `gd.AIDebug.StateTree 1`, Rewind Debugger) → **PROJECT_REFERENCE** "AI: State Tree" 섹션.

## GAS (Gameplay Ability System)

**현재 운영 (Phase 0~6 완료)**:
- 데미지/속성/공격 일원화: `UAOSAbilitySystemComponent` + `UAOSAttributeSet`(Health/MaxHealth/AttackPower/AttackRange/AttackSpeed/MoveSpeed/Damage(메타)). 캐릭터·구조물 모두 ASC 소유(PlayerState 미사용). 기본값 Health=100/AP=10/Range=500/AS=1.0/MS=600.
- **단일 데미지 흐름**: 모든 데미지가 `GE_Damage` → `AOSAttributeSet::PostGameplayEffectExecute` 통과(Health 차감 + Owner 분기로 `OnCharacterDeath`/`OnStructureDestroyed`). HP바·데미지숫자 등 비주얼 훅은 여기 한 곳.
- **기본 공격** = `UGA_Attack`(montage-driven, `Ability.Attack.Basic`, AttackSpeed 가 재생속도+쿨다운(1/AS)에 반영). 쿨다운 단일 진실 = `Cooldown.Attack.Basic` 태그.
- **스킬 = 데이터 주도**: `UGA_SkillBase`(C++ abstract) + **BP child 자산**(`BP_GA_<Char>_<Slot>` + `BP_GE_Cooldown_*`). **새 캐릭터/스킬 = BP 자산만 작성(C++ 빌드 불필요).** 절차: [`Guides/03_Implementation/SKILL_AUTHORING_GUIDE.md`](Guides/03_Implementation/SKILL_AUTHORING_GUIDE.md).
- **속성 초기값** = DataTable (`DT_CharacterAttributes`/`DT_TowerAttributes`/`DT_CommandCenterAttributes`, row 타입 `FAOSAttributeInitRow`) → float 멤버 fallback. ⚠️ 생성자에서 `MaxWalkSpeed` 강제할당 금지(BP override 존중).

**⚠️ UE 5.4+ GameplayEffect Component 패턴 (필수)** — cooldown/태그 부여 GE 는 이 패턴이 아니면 cooldown 시스템이 태그를 인식 못 함(`grants no tags` 경고 + 매 frame 활성화):
```cpp
#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"
// 생성자 안 — FindOrAddComponent/AddComponent 는 NewObject 호출이라 CDO 생성자에서 fatal.
//   CreateDefaultSubobject + GEComponents.Add 패턴을 써야 함.
UTargetTagsGameplayEffectComponent* TagsComp =
    CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(TEXT("TargetTagsGEComp"));
FInheritedTagContainer C; C.Added.AddTag(FGameplayTag::RequestGameplayTag("Cooldown.Attack.Basic"));
TagsComp->SetAndApplyTargetTagChanges(C);
GEComponents.Add(TagsComp);   // GEComponents 는 protected
```
**Instant GE 에는 TargetTagsComponent 금지** (`IsDataValid` 에러 — 즉시 만료라 grant 불가). 디버깅: `showdebug abilitysystem`, `LogAbilitySystem Verbose`.

> Phase별 마이그레이션 기록·Alex 4스킬 스펙(Q/W/E/R)·`UGA_SkillBase` UPROPERTY 전체·전체 데미지 흐름도 → **PROJECT_REFERENCE** "GAS" 섹션.

## 애니메이션 (C++ 슬롯 + BP 자산)

- `UAOSAnimInstance`(ABP parent) — `Speed/Direction/bIsMoving/bIsAttacking/bIsCasting/bIsHitReacting/bIsDead` 등 GAS 태그 미러. 생성자 `RootMotionMode = RootMotionFromMontagesOnly`.
- `AAOSCharacter` 몽타주 슬롯: `AttackMontage`/`HitReactMontage`/`DeathMontage`/`SkillMontages`(map). **nullptr 면 조용히 skip — 자산 없어도 PIE 동작.**
- **HitReact↔Attack 충돌 규칙**: (A) 공격 중(`Ability.Attack.Basic` 보유) HitReact 스킵. (B) `UGE_HitReact_State` 가 `State.HitReact` 부여(Duration=몽타주 길이, SetByCaller) → `GA_Attack::ActivationBlockedTags` 가 차단.
- **스킬 시전 root**: `UGA_SkillBase::bAllowMovementDuringCast` (BP CDO). false → `Char->ApplyCastRoot(Duration)`(`GE_Rooted` + `StopMovementImmediately`). 이동 시전은 ABP 상하체 분리(Layered Blend Per Bone, spine_01↑).

**⚠️ `OnCharacterDeath` (서버) 호출 순서 — 바꾸면 desync/root motion 깨짐:**
1. `Brain->StopLogic` (안 하면 다음 tick SendAttackEvent 가 DeathMontage 덮어씀)
2. `MOVE_NavWalking → MOVE_Walking` (NavWalking 은 root motion 무효)
3. `Multicast_PlayDeathMontage` (서버+클라 각각 재생 + 타이머 → `StartRagdoll`)
4. `SetLifeSpan(MontageLen + RagdollSettleDuration)`

**사망 시퀀스에서 건드리면 안 되는 것** (시도→되돌림): 진입 시점 capsule collision off (FindFloor 실패 → MOVE_Falling), `SetActorTickEnabled(false)`, AIController::Tick 에서 `StopMovement()`. capsule NoCollision 은 **ragdoll 진입 시점에만** 안전.

> Root motion 트러블슈팅 체크리스트(EncodeRootBoneModifier 등)·DS root motion 흐름·자산 폴더 구조 → **PROJECT_REFERENCE** "캐릭터 애니메이션 시스템".

## 경제 / 상점 / 벤픽 드래프트

- **골드** = 글로벌 팀 공유 풀 (`AOSGameState.Team1/2Gold`, 클라는 GameState 경유 조회). 적립: 캐릭터 처치 +50 / 구조물 +150 / 라운드 패시브 +100 (GameMode EditAnywhere).
- **아이템** = **유닛(UnitId=로스터 인덱스) 귀속, 라운드 누적**. 카탈로그 `DT_Items`(`FAOSItemRow`), 효과는 Infinite GE(`BP_GE_Item_*`). 캐릭터는 매 라운드 리스폰돼도 유닛 아이템 재적용. 구매는 준비/정산 단계만. ⚠️ 한 유닛은 한 슬롯에만(중복 배치 불가).
- **벤픽** = 매치당 1회 드래프트. `GetDraftSequence()` 고정 14스텝(밴4 교대 + 픽10 스네이크, 팀당 밴2·픽5). 전체 고유(한 UnitId 는 한 팀만). 이후 모든 라운드 준비는 **픽된 캐릭터만** 배치(`ServerSetLaneDeployClassesForPlayer` 서버 강제). 흐름: `Lobby→BanPick→RoundPreparation(픽 필터)→RoundRunning→Settlement→RoundPreparation`.
- 현재 로스터 20종(고유 5: 알렉스/베가/켄/캐미/가일 + 플레이스홀더 15). 알렉스만 풀스킬(Q/W/E/R), 나머지 기본 공격. 새 캐릭터 = `BP_Char_Ken` 복제 = 기본형.
- ⚠️ 위젯 실현 순서: `ShowBanPick` 에서 `InitializeWithRoster`(RootWidget 구축)를 `AddToViewport` **보다 먼저** (순서 뒤바뀌면 화면 안 뜸 — 미니맵·벤픽서 실제 발생).

> 벤픽/상점 위젯 저작 계약(`WBP_BanPick` 바인딩 이름·반응형 ScaleBox·3D 프리뷰 스테이지)·로스터 Portrait 함정 → **PROJECT_REFERENCE** "Ban/Pick" + "Economy & Shop" 섹션.

## Memory Management (필수 불변식)

**모든 `UObject*` 포인터/배열은 `UPROPERTY()` 필수** (GC — 누락 시 크래시):
```cpp
UPROPERTY()
TArray<AAOSStructure*> AllTowers;   // O    /  TArray<...> AllTowers; ← X, 크래시
```
AI 컨트롤러 소멸자에서 명시 정리:
```cpp
AAOSAIController::~AAOSAIController() { WaypointQueue.Empty(); ControlledCharacter=nullptr; CurrentTarget=nullptr; }
```

## Collision System (구조물)
- CollisionComponent / MeshComponent = `NoCollision` (캐릭터가 타워를 통과). DetectionRange = `QueryOnly`(적 감지 오버랩).

## Debug Visualization
- 런타임 `DrawDebugTowerPositions()` 는 **`AllTowers`(실제 스폰)** 순회 — `LanesInfo` 아님. 에디터 시각화는 `DrawDebugLine`(`DrawDebugBox` 는 에디터 뷰포트 미렌더).

## Dedicated Server 환경 (필수)

이 프로젝트는 **DS(Dedicated Server) 전제로 설계** — 아래 권한/로컬PC 가드 규칙은 매 작업 적용(멀티플레이 정합성). ⚠️ 단 **별도 DS 빌드 타깃(`TDProjectServer`)은 없음**: 테스트는 **PIE(Play As Dedicated Server) / Listen Server 2-Client**(규칙 7). 실제 cooked DS 빌드는 미구성.

| 시스템 | DS(서버) | 클라이언트 |
|--------|:--------:|:----------:|
| GameMode | ✅ 유일 | ❌ `GetAuthGameMode()`=null |
| GameState | ✅ 권한 | ✅ 복제 읽기전용 |
| AIController | ✅ 전부 | ❌ 없음 |
| PlayerController | ✅ 서버사이드 | ✅ 로컬 |
| Character/Structure | ✅ 스폰/파괴 권한 | ✅ 복제본 |
| UI/카메라 | ❌ 생성 금지 | ✅ `IsLocalPlayerController()` 안에서만 |
| VFX/사운드 | ❌ 렌더 없음 | ✅ |

**핵심 규칙**:
1. `HasAuthority()` 가드 — 상태 변경(HP/스폰/파괴/라운드)은 서버만.
2. `IsLocalPlayerController()` 가드 — 모든 `CreateWidget`/`AddToViewport`/카메라 앞에.
3. 클라 로직에서 `GetAuthGameMode()` 금지 → **GameState 경유**.
4. 디버그 출력(`AddOnScreenDebugMessage`/`DrawDebug`)은 DS 렌더 없음 → NetMode 체크 또는 클라 RPC.
5. 리플리케이션 = `UPROPERTY(ReplicatedUsing=OnRep_*)` + `DOREPLIFETIME` + `OnRep_*` 세트.
6. 클라→서버 = `UFUNCTION(Server, Reliable)`. 서버→전클라 = `NetMulticast` 또는 Replicated+OnRep.
7. **테스트는 Play As Dedicated Server / Listen Server 2-Client** (Single Process 는 DS 재현 부정확).

**안티패턴**: 클라에서 `GetAuthGameMode<AAOSGameMode>()->GetCurrentRound()` (null 크래시) → `GetGameState<AAOSGameState>()->GetCurrentRound()`.
**"클라에서만" 증상** = 권한 가드 누락 / 초기 복제 미전송(CDO 기본값과 같은 프로퍼티는 전송 안 됨) 의심.

## Known Issues (live)

- **지형/Nav 변경 후 RecastNavMesh 반드시 재빌드**: `RuntimeGeneration=Static` 이라 cooked. 지형 액터 위치/스케일·`NavMeshBoundsVolume`·타워 위치 변경 후 **Build → Build Paths Only(Ctrl+Shift+B)** + nav uasset 저장/커밋. 안 하면 AI 가 옛 영역에 갇히거나 정지.
- **라인 시작 = SpawnPoint 단일 진실** (위 Map/Lane 참고 — `FLaneInfo` 좌표 이중화 제거됨).

> 해결된 historical 이슈(에디터 종료 크래시, 타워 미표시, AI 미이동, HP바 미표시, 생존 캐릭터 소멸, 클라 구조물 유령 등) → **PROJECT_REFERENCE** "Known Issues and Gotchas".

## Multi-Agent Workflow (3도메인)

`.claude/agents/<name>.md` 서브에이전트 + `Task(subagent_type)`. 오케스트레이션 스킬: `/multi-agent`, `/merge-agents`.

| 도메인 | 모델 | 에이전트(소유) |
|--------|------|----------------|
| 프로그래머 (worktree, C++) | sonnet | prog-ai(AIController) / prog-character(Character·SpawnPoint·GameMode=enum소유) / prog-object(Structure·MapManager) / prog-ui(HealthBar·PlayerController) / prog-anim(AnimInstance·Notify) / build-verify(읽기전용) |
| 기획자 (MCP) | opus | design-balance(BP 파라미터) / design-level(레벨 배치) / design-docs(CLAUDE.md·Guides) |
| 아트 (MCP) | haiku | art-visual(머티리얼·텍스처·메시) / art-vfx(Niagara·UI) / art-anim(ABP·몽타주) |

- **머지 순서**(결합도 역순): docs → prog-ui → prog-anim → prog-object → prog-character → prog-ai, 각 단계 후 build-verify.
- 조율 파일: `.claude/coordination/` (INTERFACE_CONTRACTS / AGENT_STATUS / CROSS_DOMAIN_REQUESTS / ASSET_OWNERSHIP).
- ⚠️ enum 변경은 main 에 먼저 커밋 후 브랜치 (위 Enums 게이트).

## 테스트 / 검증 (피드백 루프)
- C++ Automation Test: `Source/TDProject/AOS/Tests/` (`WITH_DEV_AUTOMATION_TESTS` 가드, 별도 모듈 불필요). 첫 예시 = `AOSDraftSequenceTest.cpp`.
- 헤드리스 실행: `UnrealEditor-Cmd.exe <uproject> -ExecCmds="Automation RunTests TDProject.AOS; Quit" -unattended -nullrhi -nosplash -log`. 또는 에디터 Tools → Session Frontend → Automation.
- 순수 로직(UWorld 불필요)부터 테스트화 권장: 드래프트 시퀀스, 골드 산식, 웨이포인트 큐 순서, 아이템 귀속.

## Git Workflow
한글 제목 + 상세 한글 설명 + 푸터:
```
🤖 Generated with [Claude Code](https://claude.com/claude-code)
Co-Authored-By: Claude <noreply@anthropic.com>
```

## Language Notes
문서/주석/로그는 한·영 혼용. 사용자 요청 시 기술 설명의 한국어 번역 제공.

## 문서 인덱스
- [`Guides/01_GameOverview/PROJECT_REFERENCE.md`](Guides/01_GameOverview/PROJECT_REFERENCE.md) — **전체 상세 레퍼런스** (이 파일 모든 섹션의 풀버전 + historical)
- [`Guides/01_GameOverview/GAME_VISION.md`](Guides/01_GameOverview/GAME_VISION.md) — 게임 비전/로드맵
- [`Guides/05_ProgressLog/TIMELINE.md`](Guides/05_ProgressLog/TIMELINE.md) — 작업 타임라인
- [`Guides/03_Implementation/SKILL_AUTHORING_GUIDE.md`](Guides/03_Implementation/SKILL_AUTHORING_GUIDE.md) / [`AI_3D_ASSET_PIPELINE.md`](Guides/03_Implementation/AI_3D_ASSET_PIPELINE.md)
- [`Mcp_Tools/Anim_Pipeline/README.md`](Mcp_Tools/Anim_Pipeline/README.md) — **애니 시각 피드백 루프** (헤드리스 콘택트 시트 렌더 + 수치 QA — 애니 작업 시 이 루프 사용)
- [`Guides/DOCUMENTATION_INDEX.md`](Guides/DOCUMENTATION_INDEX.md) — 전체 문서 마스터 인덱스
