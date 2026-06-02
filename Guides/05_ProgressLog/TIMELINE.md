# TDProject — 작업 타임라인

> **목적**: 발표·회고·이력 관리용. 의미 있는 작업(기능/리팩토링/트러블슈팅/환경/도구)을 시간 순으로 기록.
> **자동 갱신 규칙**: `CLAUDE.md` 의 "## 작업 타임라인 자동 갱신" 섹션 참고. 의미 단위로 묶어 작업 완료 시점에 항목 추가.
> **항목 형식**: `작업 내용` · `문제점/난관` · `해결 방법` · `결과/영향` (+ 관련 커밋 해시).

---

## 2026-04-27 ~ 04-30 — MCP 환경 표준화 + 멀티 에이전트 구조 + SpawnPoint 단일화

**작업 내용**
- 다른 컴퓨터에서 동일하게 작업 가능하도록 MCP(unreal-engine + unreal-rag) 셋업 표준화
- `.claude/agents/` 서브에이전트로 마이그레이션, 도메인별 모델 분기(Sonnet/Opus/Haiku)
- `FLaneInfo` 의 `Team*StartPosition` 제거 → SpawnPoint 를 라인 시작 위치의 단일 진실 공급원으로
- 12개 SpawnPoint 정규화(중복/잘못된 인덱스 정리)

**문제점**
- 엔진 설치 경로가 머신마다 달라 빌드 명령·MCP 설정 하드코딩이 불가능
- 라인 시작 좌표가 `FLaneInfo` 와 `AAOSSpawnPoint` 양쪽에 이중 저장 → MCP 자동화로 한쪽만 변경되면 desync

**해결 방법**
- `UE_ROOT` 환경변수 + `claude_desktop_config.example.json` 템플릿 + `Mcp_Tools/README.md` 셋업 가이드
- `AAOSMapManager::GetLaneStartPosition` 내부를 `GameMode->GetNearestSpawnPoint` 경유로 변경

**결과**
- 새 컴퓨터에서 약 5분 셋업, 라인 자동화 시 desync 0
- 커밋: `f61a6e4`, `d279ccb`, `3abf9e8`, `0ed299a`

---

## 2026-05-01 ~ 05-02 — ServerTravel race + NavMesh stale 수정

**작업 내용**
- 라운드 진행을 위한 `ServerTravel` 직후 안정성 확보
- 지형 4배 확장 작업 + 누락된 저장 복구

**문제점**
- 레벨 전환 직후 `MapManager` nullptr crash (초기화 race)
- 지형 변경 후에도 AI가 옛 영역에 갇혀 멈춤 — RecastNavMesh 가 `RuntimeGeneration=Static`(cooked)

**해결 방법**
- `MapManager` nullptr 가드 + race 재시도 로직
- 지형/Nav 영역 변경 시 **Build → Build Paths Only** 강제, `CLAUDE.md` 에 절대 규칙으로 기록

**결과**
- 라운드 전환 무중단, AI 정지 0
- 커밋: `2fe16d4`, `421e56b`, `9abad72`

---

## 2026-05-03 — GAS 도입 (Phase 0~3, 5)

**작업 내용**
- Gameplay Ability System 으로 데미지/속성/공격 일원화
- `UAOSAbilitySystemComponent` + `UAOSAttributeSet` + `UGA_Attack` + `UGE_Damage`
- DataTable 기반 속성 초기화 (Character/Tower/CommandCenter)
- Structure 도 `IAbilitySystemInterface` 구현 (Phase 5)

**문제점**
- 기존 float 멤버(Health/AttackDamage) + deprecated `ReceiveDamage` 흐름 + Structure 는 ASC 없음 → 데미지 경로 이원화
- UE 5.4+ 의 `GameplayEffectComponent` 시스템에서 cooldown GE 가 태그를 grant 못해 매 frame 활성화

**해결 방법**
- AttributeSet 의 `PostGameplayEffectExecute` 가 Owner 분기(Character/Structure)로 사망 처리
- Cooldown GE 에 `CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>` + `GEComponents.Add` 패턴

**결과**
- Damage 흐름 단일화, 공격자/피격자 모두 ASC 통과
- 커밋: `77b0d9b`

---

## 2026-05-07 — StateTree AI 마이그레이션 (Phase 6)

**작업 내용**
- `AOSAIController::UpdateAIBehavior` (if/else) 를 StateTree 로 교체
- Custom Tasks(`FindNearestEnemy`, `MoveToCurrentTarget`, `MoveToCurrentWaypoint`, `SendAttackEvent`, `ActivateAbilityByTag`) + Conditions

**문제점**
- 초기화 race 로 schema binding 실패 — `BeginPlay` 시점엔 `GetPawn()=null`
- `ContextActorClass` 를 AIController 로 잘못 지정해 binding 실패
- Running task 가 있는 state 는 자동 재선택 안 됨 → 적 만나도 안 싸움

**해결 방법**
- `bStartLogicAutomatically=false` + `OnPossess` 에서 수동 `StartLogic()`
- ContextActorClass = `AOSCharacter` (Pawn 클래스)
- Root 에 `On Tick` 트랜지션으로 우선순위 강제 재평가

**결과**
- 시각 편집 가능한 AI 행동 결정, 트러블슈팅 4종 문서화
- 커밋: `761b700`, `684c255`

---

## 2026-05-12 — 애니메이션 슬롯 + Death Montage Root Motion + Ragdoll (Phase 3.5)

**작업 내용**
- 캐릭터 애니 스캐폴딩: `UAOSAnimInstance`, 4개 몽타주 슬롯, AnimNotify
- 사망 모션: 루트모션으로 마지막 발걸음 → 랙돌 전환

**문제점**
- 사망 몽타주가 InPlace 로 재생됨 — RootMotionMode 기본값이 `NoRootMotionExtraction`
- 외부 AnimSequence 의 bone track 이 비어 있고 root motion 데이터가 curve 에만 있음
- `MOVE_NavWalking` 이 root motion delta 를 snap-to-navmesh 로 무효화
- StateTree 가 다음 틱에 `SendAttackEvent` 로 `AttackMontage` 를 재생해 `DeathMontage` 덮어씀
- 사망 진입 시 capsule collision 끄면 `FindFloor` 실패 → `MOVE_Falling` 으로 흐름 깨짐

**해결 방법**
- `RootMotionMode = RootMotionFromMontagesOnly` 생성자 설정
- `EncodeRootBoneModifier` (pelvis → root) baking — 자산 복제 후 적용
- `OnCharacterDeath` 호출 순서: ① `Brain->StopLogic` ② `MOVE_NavWalking → MOVE_Walking` ③ `Multicast_PlayDeathMontage` ④ `StartRagdoll` 타이머에서만 capsule NoCollision
- 사망 시퀀스 동안 capsule/Tick 절대 끄지 않음

**결과**
- 자연스러운 루트모션 사망 → 랙돌 정착
- 커밋: `42dbf14`

---

## 2026-05-15 — HitReact 충돌 규칙 + Montage-driven Damage + AttackSpeed 동적 적용

**작업 내용**
- 데미지 발생 시점을 AnimNotify 와 동기화 (`WaitGameplayEvent("AnimNotify.AttackHit")`)
- `AttackSpeed` 속성이 몽타주 재생속도(Rate) + 쿨다운(1/AttackSpeed) 양쪽에 반영
- HitReact ↔ Attack 충돌 규칙 두 가지

**문제점**
- 같은 DefaultSlot 에서 HitReact 가 Attack 을 강제 중단 → 공격이 끊김
- AI 가 HitReact 도중에도 공격을 시도해 부자연
- `AttackSpeed` 가 실제 게임플레이에 거의 영향 없었음

**해결 방법**
- Rule A: `Multicast_PlayHitReact` 진입 시 `Ability.Attack.Basic` 태그 보유면 HitReact 스킵
- Rule B: `UGE_HitReact_State` 가 `State.HitReact` 부여 → `GA_Attack::ActivationBlockedTags` 가 차단
- SetByCaller 로 HitReact GE 의 Duration 을 몽타주 길이로 동적 설정

**결과**
- 공격 끝까지 재생 + 데미지 타이밍 시각 일관, AttackSpeed 가 게임플레이에 직접 영향
- 커밋: `47137cc`

---

## 2026-05-26 — Alex 4스킬 (Phase 4) + Git LFS

**작업 내용**
- 캐릭터당 LoL 3스킬 + 1궁극기 — Alex = 가렌 Q(DecisiveStrike) / W(Courage) / E(Judgment) / R(DemacianJustice)
- `GA_Alex_Q/W/E/R` C++ 4종 + 쿨다운 GE 4종 + 효과 GE(`MoveSpeed_Boost`, `DamageShield`, `EnhancedAttack`)
- StateTree 에 `UseR/W/Q/E` state + 우선순위 (Design B)
- Git LFS — `.uasset/.umap` 추적 (forward-only)

**문제점**
- StateTree 에서 스킬이 발동 안 됨 — Root 의 On Tick 전환이 AttackEnemy 로만 강제 → 스킬은 평가도 안 됨
- `HasCooldownTag` 의 `bInvert` 기본값 false → 거꾸로(쿨다운 중에만 통과) 동작
- `FStateTreeTask_ActivateAbilityByTag` 가 `TriggerEventData` 를 전달 안 함 → R 이 Target null 로 데미지 0
- ParagonKwang 2.4GB 가 GitHub 무료 LFS 1GB 한계 초과

**해결 방법**
- Design B: running state(PushLane/AttackEnemy)만 `On Tick → Root` 재선택, 우선순위는 자식 순서, PushLane 맨 마지막
- 모든 스킬 EnterCondition 의 `HasCooldownTag.bInvert = true`
- R 에 AIController 타겟 fallback (`GetCurrentTargetCharacter` → `FindNearestEnemy`)
- LFS 전략 = "프로젝트 에셋만 추적 + forward-only, 대용량 외부 자산은 gitignore"

**결과**
- 4스킬 정상 발동 + 외부 자산 의존성 해소
- 커밋: `7bab478`, `9b9ab32`

---

## 2026-05-26 ~ 05-28 — 환경 트러블: BSOD 0x50 (cbfltfs4.sys / MarkAny ePageSafer)

**작업 내용**
- UE 에디터 + Claude Desktop 동시 실행 시 강제 재부팅 5회 진단/해결

**문제점**
- 외관상 RAM/PSU 의심 (UE 실행 시점에 터지므로) — 무관한 방향으로 시간 낭비 위험

**해결 방법**
- Event Viewer → BugCheck 1001 + WHEA 없음 + 디스크 SMART 정상 확인
- WinDbg(`cdb`) 설치 → `!analyze -v` → `IMAGE_NAME: cbfltfs4.sys` + `PROCESS_NAME: python.exe`(unreal-rag MCP) 식별
- 드라이버 추적: 2017년 EldoS CBFS Filter, 소유 앱 = MarkAny ePageSafer (이미 삭제됨, 드라이버만 잔재)
- `sc.exe config cbfltfs4 start= disabled` + 재부팅

**결과**
- BSOD 0회 + 가이드 문서화 (`Guides/04_Testing/TROUBLESHOOTING_BSOD_cbfltfs4.md`)
- 커밋: `322dd3b` (문서 포함)

---

## 2026-05-29 — unreal-rag 자동 stale 감지

**작업 내용**
- `ue_rag_mcp.py` 가 코드 변경 시 자동 재인덱싱

**문제점**
- 기존 로직이 `chroma_db` 존재 시 로드만 하고 재인덱싱 경로 없음 → 최초 인덱싱 이후 수정한 모든 코드가 검색 누락 → 디버깅 시간 손실

**해결 방법**
- 인덱싱 시 최신 소스 `mtime` 을 `chroma_db/index_meta.json` 에 기록
- 시작 시 현재 `Source/` 최신 mtime 과 비교 → 더 최신이면 전체 재인덱싱 (구버전 인덱스는 메타 없음 → stale 처리)

**결과**
- 코드 수정 → MCP 재시작만으로 인덱스 자동 갱신
- 커밋: `322dd3b`

---

## 2026-05-29 — 시전 root + 상하체 분리 Part A (Phase 4+ C++)

**작업 내용**
- 스킬별 "시전 중 이동 가능 여부" 플래그(`bAllowMovementDuringCast`) 도입
- `GE_Rooted` + `AOSCharacter::ApplyCastRoot` + `FStateTreeTask_ActivateAbilityByTag` 의 State.Rooted 대기 로직
- `bIsCasting` 태그 미러로 ABP 상하체 분리 트리거 준비

**문제점**
- 스킬 발동 시 슬라이드(StateTree task 즉시 Succeeded → AI 이동 재개 + DefaultSlot 전신 캐스트 애니가 캡슐 이동 위에 그대로 재생)
- 이동 불가 스킬도 AI 가 즉시 다른 상태로 빠짐 → 시각/논리 부조화

**해결 방법**
- 플래그 1개로 root 적용 여부 결정 (Q/W=true, E/R=false)
- StateTree task 가 `State.Rooted` 보유 중엔 RUNNING 유지(EnterState+Tick) → 이동 불가 스킬은 AI 가 끝까지 홀드
- ABP 신호용 `State.Casting` 태그 (4개 스킬 공통 `ActivationOwnedTags`)

**결과**
- Part A C++ 완료 + 커밋. Part B(ABP `Layered Blend Per Bone` + UpperBody 슬롯)는 진행 중.
- 커밋: `322dd3b`

---

## 2026-06-02 — 데이터 주도 스킬 마이그레이션 3단계 (cleanup) 완료

**작업 내용**
- 구버전 C++ 클래스 8개(파일 16개) 제거 — git rm:
  - `Source/TDProject/AOS/GAS/Abilities/GA_Alex_{Q,W,E,R}.h/cpp` (4쌍)
  - `Source/TDProject/AOS/GAS/Effects/GE_Cooldown_Alex_{Q,W,E,R}.h/cpp` (4쌍)
- CLAUDE.md "Phase 4" 섹션 상단에 historical note 추가 (스펙 참고용으로만 유지)
- CLAUDE.md 마이그레이션 상태 표를 1·2·3 단계 모두 ✅ 로 표기
- CLAUDE.md "Phase 4+ 시전 root" 섹션의 `GA_Alex_*.h` 참조를 `UGA_SkillBase` 로 갱신
- 외부 참조 점검: AOSAnimInstance.cpp/GE_SkillCooldown_Base.h 의 코멘트 2개가 옛 클래스 언급 → 현 패턴 기준 문구로 갱신 (코드 의존성 0)

**문제점 / 난관**
- 캐릭터당 8개씩 늘어나는 C++ 클래스 폭증을 막기 위해 데이터 주도로 전환했으나, 마이그레이션 동안 옛+새가 병행 운영 → 정리 누락 시 의도치 않은 이중 활성화 위험
- 단순 삭제 전에 잔존 참조(코멘트 포함) 모두 grep 으로 확인 필요

**해결 방법**
- 점진적 마이그레이션 후 PIE 검증 통과 시점에 일괄 삭제
- 코멘트는 historical 가치 있는 곳만 과거 시제로 정리, 나머지는 BP 패턴 기준으로 갱신

**결과**
- C++ 클래스 수: 8 (구) → 0 (현). 캐릭터 추가 시 BP 자산만 작성. 향후 N캐릭터·M스킬 확장 시 코드 0줄 추가.
- 마이그레이션 전체(1+2+3) 완료
- 관련 가이드: `Guides/03_Implementation/SKILL_AUTHORING_GUIDE.md`

---

## 2026-06-02 — GA_Attack 인스턴스 영구 stuck race 디버그/픽스

**작업 내용**
- 데이터 주도 스킬 마이그레이션 후 PIE 검증 중 발견된 race 진단
- 3중 방어 픽스 적용:
  1. `GA_Attack::ActivationBlockedTags` 에 `State.Casting` 추가 — 스킬 시전 중 새 Attack 활성화 차단
  2. `UGA_SkillBase::ActivateAbility` 진입 시 `ASC->CancelAbilities({Ability.Attack.Basic})` — 살아있는 Attack 강제 종료
  3. `GA_Attack` 의 PlayMontageAndWait `bAllowInterruptAfterBlendOut: false → true` — Interrupted 콜백 누락 race 근본 차단

**문제점 / 난관**
- 첫 만남에서 일반 공격 1회 발동 후, Q 한 번 쓰면 이후 일반 공격 영영 안 나옴 — Q 가 잘 활성화되는데도 일반 공격만 dead
- 로그: `Can't activate ... Default__GA_Attack ... already a currently active instance` 가 매 틱 반복
- `showdebug abilitysystem`: Q 가 모두 끝났는데도 (`State.Casting`/`State.Rooted`/`Ability.Skill.Alex.Q` 다 해제) `Ability.Attack.Basic` 만 영구히 남음
- 원인: GA_Attack 의 `PlayMontageAndWait` 가 Q 몽타주에 의해 같은 슬롯/blend out 타이밍에 interrupt 될 때 `OnInterrupted` 콜백 누락 → `EndAbility` 미호출 → 인스턴스 영구 active (`InstancedPerActor` 라 재활성화 불가)

**해결 방법**
- 새 Attack 차단(State.Casting) 만으로는 부족 — **이미 stuck 된 인스턴스** 처리 못 함
- 스킬 시작 시 `CancelAbilities` 로 명시적 정리 + PlayMontageAndWait 자체 race 도 `bAllowInterruptAfterBlendOut=true` 로 차단
- 3중 방어가 서로 보완 — 어느 하나가 실패해도 나머지가 커버

**결과**
- Q ↔ 일반 공격 정상 사이클 복구 ("잘 공격하고 있어" 확인)
- 다음 검증 필요: W/E/R 도 동일하게 잘 동작하는지

---

## 2026-06-02 — 게임 방향성 확정 (오토배틀러 MOBA) + 비전 문서

**작업 내용**
- 게임 정체성을 명문화: **드래프트로 5캐릭 뽑아 라인 배치 → 라운드 사이 글로벌 골드로 아이템 구매 → AI 전투 관전 → 적응**하는 오토배틀러 MOBA (북극성: Mechabellum)
- 라운드 모델 확정: **맵 누적** (캐릭터는 라운드마다 리셋·재배치, 타워/CC HP는 누적)
- 디자인 기둥 4개 정립 (전략이 곧 플레이 / 관전이 읽혀야 함 / AI가 캐릭터 개성 연기 / AI로 만든·AI를 지휘하는 게임)
- 임계 경로 로드맵 Slice 0~4 (심장박동 → 가독성 → AI 깊이 → 드래프트 → 매크로)
- `Guides/01_GameOverview/GAME_VISION.md` 신규 — "무엇을 만드는가"의 단일 진실 공급원

**문제점 / 난관**
- 코드에 "누가 무엇을 하는 게임인가"가 드러나지 않았음 — 시스템(GAS/StateTree/데이터주도)은 두꺼운데 코어 재미 루프 정의가 약했음
- 플레이어 입력이 *상점+배치* 둘뿐 → 결정→결과 인과가 안 읽히면 전략적 쾌감이 안 생기는 구조적 리스크

**해결 방법**
- 방향성 점검 세션으로 코어 루프·플레이어 동사·라운드 모델을 명시적으로 정의
- 핵심 재정의: "관전하는 게임이라 **AI의 똑똑함보다 AI의 가독성**이 게임 그 자체" → 미니맵·결과요약을 장르 필수 인프라로 승격
- 이미 구현된 토대(AOSPlayerState.DeployCount*, 라운드 전환, 데이터주도 스킬)와 비전을 매핑해 진짜 빈칸(상점/골드/라인승패판정/AI깊이/미니맵)을 특정

**결과**
- 후속 모든 작업의 우선순위 판단 기준 확보 (GAME_VISION 이 1순위 문서)
- 다음 작업: Slice 0 (심장박동 — 골드 경제 + 최소 상점 + 아이템 1~2개)
- 열린 튜닝 포인트 기록: 2라인 재배치 룰 방향, 골드 스노볼 여부, 로스터 규모
- 관련 문서: `Guides/01_GameOverview/GAME_VISION.md`

---

## 2026-06-02 — Slice 0: 골드 경제 + 아이템 상점 메커니즘 (심장박동 검증 통과)

**작업 내용**
- 비전 로드맵 Slice 0 ("심장박동 증명") 구현 — *골드 벌고 → 아이템 사고 → 캐릭터 강해진다* 루프
- **골드 (`AOSGameState`)**: `Team1/Team2Gold` (Replicated + OnRep + 델리게이트), `ServerAddGold/ServerSetGold/GetGold`
- **적립 (`AOSGameMode`)**: 캐릭터 처치 시 상대 팀 +50, 구조물 파괴 시 +150, 라운드 시작 패시브 +100 (전부 EditAnywhere 튜닝). killer 추적 없이 "죽은 쪽 반대 팀" 으로 단순화
- **아이템 (`FAOSItemRow` DataTable + `AOSGameMode`)**: `(팀,라인)→구매 아이템 GE 목록` 서버 인벤토리. `SpawnCharactersForRound` 에서 스폰 직후 재적용 → **라운드 리셋돼도 누적**. `ServerBuyLaneItem`(준비/정산 단계 + 골드 검증), `BuyItem` 콘솔 exec(테스트), `Server_BuyLaneItem` RPC(위변조 방지=서버가 PlayerState 팀 강제)
- **BP 자산** (agent-design-balance): `BP_GE_Item_Sword`(AttackPower+25, Infinite), `BP_GE_Item_Vitality`(MaxHealth+200), `DT_Items`(Sword/Vitality 각 100골드), `TDProj_GM.ItemTable` 연결

**문제점 / 난관**
- 구매 시 "ItemTable 미설정" 로그 — 아이템이 안 사짐
- 원인 추적: World Settings 오버라이드는 TDProj_GM_C 로 정상인데도 실패 → `DefaultEngine.ini` 의 **`GlobalDefaultGameMode=/Script/TDProject.AOSGameMode` (C++ 스태틱 클래스)** 발견. BP `TDProj_GM` 의 모든 설정(ItemTable/Roster)이 빠진 raw C++ 인스턴스가 특정 경로에서 활성화됨
- 추가로 design-balance 가 Python `set_editor_property` 로 CDO 만 바꾸고 BP 재컴파일을 안 해 stale CDO 가능성

**해결 방법**
- `DefaultEngine.ini` → `GlobalDefaultGameMode=/Game/TDProj_GM.TDProj_GM_C` (BP 로 교체)
- TDProj_GM 강제 재컴파일 + 저장 (ItemTable 을 generated class CDO 에 확정)
- 사용자 직감("스태틱 클래스가 들어가 있다")이 정확했음 — 진단의 결정적 단서

**결과**
- PIE 검증 통과: 아이템 정상 구매 + 캐릭터 점점 강해짐 확인 → **게임의 핵심 루프 작동 증명**
- 다음(Slice 1 — 가독성): 미니맵 / 라운드 결과 요약 / 데미지 피드백. 비전 기둥 2 "관전이 읽혀야 한다"
- 관련 문서: `Guides/01_GameOverview/GAME_VISION.md` (로드맵)

---

## 2026-06-02 — DS 클라이언트 시각 버그 4종 수정 (HP바·구조물 파괴·로그)

**작업 내용**
- #1 로그 스팸: StateTree task EnterState 로그(MoveToCurrentWaypoint/Target, SendAttackEvent) Warning→Verbose (Design B 재선택으로 매 틱 도배)
- #2 캐릭터 HP바 팀 색상: `Team` 을 `ReplicatedUsing=OnRep_Team` 으로 + `RefreshHealthBarTeamColor`(Team1=Red/Team2=Blue) 를 OnRep + Tick lazy 적용 (DS 클라가 팀을 알아야 색칠 가능)
- #3 구조물 파괴 시 클라 메시 잔존 + #4 HP바 드레인 깜빡임 — **단일 근본 원인 해결**
- HP바 위젯 일반 개선: World-space 양면 렌더(`SetTwoSided`), Masked 블렌드, 드레인부 불투명 배경

**문제점 / 난관 (디버깅 여정)**
- #3·#4 가 **클라에서만** 발생 (호스트 정상) → 네트워킹 문제로 좁힘
- 멀티캐스트·Health델리게이트·복제 bool(OnRep) 등 여러 숨김 경로를 넣어도 클라 메시가 안 사라짐 → 진단 로그로 실측
- 로그 분석 결과 **`MapManager::SpawnStructures` 가 클라에서도 실행** (권한 가드 없음) → 클라가 구조물을 **로컬 중복 스폰**. 클라엔 구조물 2벌(로컬 유령 + 서버 복제본)이 존재:
  - #3 = 서버가 못 건드리는 로컬 유령 메시가 남음
  - #4 = HP바 2개(로컬 풀 + 복제본 드레인)가 겹쳐 깜빡임
- 클라 중복 제거 후엔 복제본에 메시가 없음(`SetupTowerMesh` 는 서버 전용 `Initialize` 에서만 호출) → Team1(레드)만 안 보임
- 그 원인: UE 초기 복제는 **CDO 기본값과 다른 프로퍼티만 전송** → Team1/Tower(둘 다 기본값)는 `OwnerTeam`/`StructureType` 이 전송조차 안 됨 → OnRep(REPNOTIFY_Always 로도) 미발화

**해결 방법**
- `MapManager::SpawnStructures` 를 `if (HasAuthority())` 가드 → 서버만 스폰, 클라는 복제 수신 (유령·중복 제거 = #3·#4 동시 해결)
- 클라 복제 구조물 메시: `BeginPlay` 에서 `if (!HasAuthority()) SetupMeshForCurrentType()` 직접 호출 (이 시점엔 복제 프로퍼티 적용됨 + 기본값은 생성자 기본과 동일 → 전 구조물 정확). OnRep_StructureType/OwnerTeam 은 보정·색상용
- `bDestroyedVisual` 복제 bool + late-join 가드(파괴된 구조물 재표시 방지)

**결과**
- 클라에서 양 팀 타워·CC 정상 표시 + 파괴 시 즉시 사라짐 + HP바 깜빡임 없음 (PIE 리슨서버 확인)
- 보너스: 클라가 스폰하던 유령 구조물 ~20개 제거 → 클라 성능·정확성 개선
- 교훈: "클라에서만" 증상 = 권한 가드 누락/초기 복제 미전송 의심. 증상별 땜질보다 NetMode 별 실행 경로 추적이 빠름

---

## 진행 중 (작업 완료 시 위 형식으로 이동)

### Part B — ABP 상하체 분리 배선
- UpperBody 슬롯 생성 + 4개 스킬 몽타주 재배치
- AnimGraph: `Save Cached Pose 'UpperFull'` + 2× `Use cached pose` 로 fan-out, `Layered blend per bone(spine_01)` + `Blend Poses by bool(bIsMoving)`
- 진행 상황: 캐시 포즈 fix 적용 중 (`Slot 'UpperBody'` 출력의 직접 fan-out 불가 → cached pose 경유 필요)
- 문제점/난관: AnimGraph 포즈 핀이 "출력→입력 1개"만 가능한 제약을 처음 만나 우회 패턴 발견
- 관련 가이드: `Guides/03_Implementation/UPPER_LOWER_BODY_SPLIT.md`

### 스킬 데이터 주도 리팩토링 — `UGA_SkillBase` (1·2단계 완료, 3단계 대기)

**작업 내용**
- 캐릭터 개별 스킬 = C++ 1개 base + BP child 자산으로 생산 구조 전환
- **1단계 ✅**: `UGA_SkillBase`(데이터 주도 부모 GA) + `UGE_SkillCooldown_Base`(쿨다운 GE 부모) C++ 작성
  - UPROPERTY: SkillIdentityTag, bAllowMovementDuringCast, CooldownDuration, SelfAppliedEffects[], TargetAppliedEffects[], DamageGameplayEffectClass, TargetType(Self/SingleEnemy/AoE_Sphere), AoERadius, BaseDamage, MissingHpDamageScale, PeriodicTickCount/Interval
  - 통합 ActivateAbility: 쿨다운→Commit→Self GE→Target 분기→Cast Root→Montage→EndAbility
  - BlueprintNativeEvent `CalculateTargetDamage` (R 의 처형식 등 BP override 가능)
  - `PostInitProperties/PostLoad` 가 SkillIdentityTag 를 AssetTags(`GetAssetTags()`/`SetAssetTags()` 신 API) + ActivationOwnedTags 에 자동 추가
- **2단계 ✅** (agent-design-balance + agent-design-docs 병렬):
  - **BP 자산 8개**:
    - `/Game/AOS/GAS/Effects/Cooldowns/Alex/BP_GE_Cooldown_Alex_{Q,W,E,R}` (parent `UGE_SkillCooldown_Base`, GrantedTag `Cooldown.Skill.Alex.*`)
    - `/Game/AOS/GAS/Abilities/Alex/BP_GA_Alex_{Q,W,E,R}` (parent `UGA_SkillBase`, UPROPERTY 풀세팅)
  - **BP_Char_Alex.StartupAbilities** 4 C++ → 4 BP 교체 (`GA_Attack` C++ 는 유지)
  - **신규 가이드**: `Guides/03_Implementation/SKILL_AUTHORING_GUIDE.md` (~330줄, 새 스킬 5~10분 작성 절차)
  - 기존 C++ 4종(`GA_Alex_*` + `GE_Cooldown_Alex_*`) 은 그대로 보존 — BP 만 부여되므로 충돌 없음
- **3단계 🔜**: 기존 C++ 4종 cleanup 제거 (PIE 검증 통과 후)

**문제점 / 난관**
- 캐릭터당 4 C++ + 4 쿨다운 GE → 캐릭터 N개로 늘면 코드 폭증, 디자이너가 새 스킬 추가하려면 C++ 빌드 필요
- 빌드 시 `TObjectPtr<const T>::IsValid` 멤버 없음 에러 (FGameplayEventData::Target 패턴 미숙지) + UE 5.7 의 `AbilityTags` deprecation 경고
- `UTargetTagsGameplayEffectComponent::SetAndApplyTargetTagChanges()` Python 노출 안 됨, 속성 이름 `InheritableGrantedTagsContainer` (가이드의 `InheritableOwnedTagsContainer` 와 다름)

**해결 방법**
- C++ 1 base + BP child 패턴 (UE 표준). 통합 흐름이 Self-buff(Q/W) + 단일 처형(R) + AoE 주기(E) 모두 커버, 특이 로직만 BP `CalculateTargetDamage` override
- `Target.Get()` 후 null 체크로 IsValid 우회, `GetAssetTags()/SetAssetTags()` 신 API 로 마이그레이션
- 컴포넌트 추가 + GrantedTag 설정은 `unreal.EditorAssetLibrary.rename_asset` 등 Python 직접 호출로 우회

**결과**
- 캐릭터·스킬 확장 시 BP 만으로 작성 가능 → 디자이너 이터레이션 시간 대폭 단축
- C++ 클래스 수 = 1 (base) + 1 (cooldown base) — 기존 4+4=8 대비
- 3단계 cleanup 후 기존 C++ 8개 모두 제거 예정 → 코드 베이스 ↓
- 관련 가이드: `Guides/03_Implementation/SKILL_AUTHORING_GUIDE.md`
