# 스킬 작성 가이드 — `UGA_SkillBase` 기반

**작성일**: 2026-06-01
**대상**: 디자이너 / `design-balance` / `prog-character` / 신규 캐릭터 작업자
**관련 Phase**: Phase 4+ (스킬 데이터 주도 리팩토링 2단계)
**관련 문서**:
- [UPPER_LOWER_BODY_SPLIT.md](./UPPER_LOWER_BODY_SPLIT.md) — 시전 root + 상하체 분리
- `CLAUDE.md` → "스킬 데이터 주도식 — `UGA_SkillBase`" 섹션
- [TIMELINE.md](../05_ProgressLog/TIMELINE.md) — 리팩토링 이력

---

## 한눈에 보기

> **새 스킬 1개 = BP 자산 2개 (스킬 GA + 쿨다운 GE) + 태그 2개 + 몽타주 매핑 1개**
> C++ 변경 / 빌드 / 에디터 재시작 **불필요**.

```
[C++ 1개 base]  UGA_SkillBase
                    ▲ 상속
[BP child]      BP_GA_Alex_Q     ← UPROPERTY 만 채움
                    │
                    ├─ CooldownGameplayEffectClass → BP_GE_Cooldown_Alex_Q
                    ├─ SkillIdentityTag           → Ability.Skill.Alex.Q
                    └─ (필요시) Calculate Target Damage override
```

---

## 핵심 개념

| 요소 | 역할 |
|------|------|
| `UGA_SkillBase` (`Abilities/GA_SkillBase.h`) | 데이터 주도식 부모 — UPROPERTY 만 채우면 4가지 패턴(Self / SingleEnemy / AoE_Sphere / Periodic AoE) 모두 표현 |
| `UGE_SkillCooldown_Base` (`Effects/GE_SkillCooldown_Base.h`) | 쿨다운 GE 부모 — Duration = `SetByCaller(Data.Duration)` |
| `SkillIdentityTag` | `Ability.Skill.<Char>.<Slot>` — `AbilityTags` + `ActivationOwnedTags` 자동 등록 (StateTree 가 `TryActivateAbilitiesByTag` 로 트리거) |
| `Cooldown.Skill.<Char>.<Slot>` | 쿨다운 GE 가 부여하는 차단 태그 — StateTree `HasCooldownTag(bInvert=true)` 가 검사 |
| `bAllowMovementDuringCast` | `false` = 시전 중 정지 + AI 홀드 / `true` = 즉발 후 이동 재개 (자세한 동작은 UPPER_LOWER_BODY_SPLIT.md) |

---

## 워크플로 — 새 스킬 5~10분 안에 추가

예시: **Bob 캐릭터의 Q 스킬 — 자기 이동속도 버프** 신규 추가.

### Step 1. GameplayTags 등록

`Config/DefaultGameplayTags.ini` 에 2개 추가:

```ini
+GameplayTagList=(Tag="Ability.Skill.Bob.Q",DevComment="Bob Q skill identity")
+GameplayTagList=(Tag="Cooldown.Skill.Bob.Q",DevComment="Bob Q cooldown")
```

- **식별 태그**: `Ability.Skill.<Char>.<Slot>` — 카테고리 prefix 는 반드시 `Ability.Skill`
- **쿨다운 태그**: `Cooldown.Skill.<Char>.<Slot>` — 카테고리 prefix `Cooldown.Skill`

> 태그 추가 후 에디터에서 **Project Settings → GameplayTags** 패널을 다시 열면 즉시 인식. 재시작 불필요.

### Step 2. 쿨다운 GE BP 생성

1. Content Browser → `/Game/AOS/GAS/Effects/` → **Right-click → Blueprint Class**
2. All Classes → `GE_SkillCooldown_Base` 검색 → 부모로 선택
3. 이름: **`BP_GE_Cooldown_Bob_Q`**
4. 열어서 → **Components** 패널 → **Add** → `Target Tags Gameplay Effect Component`
5. 추가된 컴포넌트 → **Inherited Tags → Added** → `Cooldown.Skill.Bob.Q`
6. 저장

> Duration 은 base 가 `SetByCaller(Data.Duration)` 로 자동 처리 — **BP 에서 건드리지 않음**. UPROPERTY `CooldownDuration` (스킬 GA) 이 set 한다.

### Step 3. 스킬 GA BP 생성

1. Content Browser → `/Game/AOS/GAS/Abilities/` → **Right-click → Blueprint Class**
2. All Classes → `GA_SkillBase` 검색 → 부모로 선택
3. 이름: **`BP_GA_Bob_Q`**
4. 열어서 → **Class Defaults** → 아래 표대로 UPROPERTY 입력 (패턴별 치트시트 참고)
5. **Cooldown Gameplay Effect Class** → `BP_GE_Cooldown_Bob_Q` 지정 (Step 2 의 자산)
6. 저장 + 컴파일

### Step 4. `BP_Char_Bob.StartupAbilities` 에 등록

1. `BP_Char_Bob` (없으면 `BP_Char_Alex` 복제 후 메시/머티리얼 교체) 열기
2. Class Defaults → **AOS|GAS|Init → Startup Abilities** 배열에 `BP_GA_Bob_Q` 추가
3. 다른 슬롯(W/E/R)도 있으면 같이 추가 (StartupAbilities 는 순서 무관)

### Step 5. 몽타주 매핑 (`SkillMontages`)

1. `BP_Char_Bob` → **AOS|Animation → Skill Montages** map (`TMap<FGameplayTag, UAnimMontage*>`)
2. **Key**: `Ability.Skill.Bob.Q`
3. **Value**: 해당 스킬의 `AM_Bob_Q` 몽타주 자산
4. 몽타주 슬롯 설정 — UPPER_LOWER_BODY_SPLIT.md 의 B-1 참고:
   - `bAllowMovementDuringCast = true` → 슬롯 = **`UpperBody`** (상체만 재생, 다리 locomotion)
   - `bAllowMovementDuringCast = false` → 슬롯 = **`DefaultSlot`** (전신)

> 매핑 없으면 데미지/효과는 적용되지만 애니가 안 나옴. 디버그 로그에 `GetSkillMontage returned null for tag ...` 와 같은 경고가 찍힌다.

### Step 6. StateTree 자식 노드 추가 (필요 시)

`ST_AOSCharacterAI` 에 `UseQ_Bob` state 가 없다면 추가. **Design B** 패턴 — 자식 노드 순서가 우선순위:

```
Root
├── UseR
├── UseW
├── UseQ_Bob  ← 추가 (조건: !HasCooldownTag(Cooldown.Skill.Bob.Q) + 사용 조건)
│             Tasks: ActivateAbilityByTag(Ability.Skill.Bob.Q)
├── ...
└── PushLane  (반드시 맨 마지막)
```

> 자세한 패턴은 `CLAUDE.md` → "StateTree 스킬 통합 — Design B" 참고.

---

## 패턴별 UPROPERTY 치트시트

기존 Alex 4스킬을 4가지 대표 패턴으로 일반화한 표. 새 스킬을 만들 때 가장 가까운 패턴을 복사해서 시작.

### 공통 (모든 스킬)

| Category | Property | 설명 |
|----------|----------|------|
| Identity | `SkillIdentityTag` | `Ability.Skill.<Char>.<Slot>` |
| Cooldown | `CooldownGameplayEffectClass` | `BP_GE_Cooldown_<Char>_<Slot>` |
| Cooldown | `CooldownDuration` | 초 단위 |
| Cast | `bAllowMovementDuringCast` | 이동 가능 = `true`, 정지/홀드 = `false` |
| Cast | `ExplicitRootDuration` | `-1` = 자동 (Periodic > Montage 길이). 양수면 강제 |

### 패턴 A — Self 버프 (Q: DecisiveStrike)

자기 자신에게 GE 적용만. 데미지 없음.

| Category | Property | 값 |
|----------|----------|-----|
| Targeting | `TargetType` | `Self` |
| Effects | `SelfAppliedEffects` | `[GE_MoveSpeed_Boost, GE_EnhancedAttack]` |
| Effects | `DamageGameplayEffectClass` | (비움) |
| Damage | `BaseDamage` | 0 |
| Cast | `bAllowMovementDuringCast` | `true` (이동하며 시전) |
| Cooldown | `CooldownDuration` | 8 |

### 패턴 B — Self 방어 (W: Courage)

자기 보호 버프. Q 와 동일한 구조, GE 만 다름.

| Property | 값 |
|----------|-----|
| `TargetType` | `Self` |
| `SelfAppliedEffects` | `[GE_DamageShield]` |
| `DamageGameplayEffectClass` | (비움) |
| `bAllowMovementDuringCast` | `true` |
| `CooldownDuration` | 15 |

### 패턴 C — Periodic AoE (E: Judgment)

caster 위치 반경에 주기적 데미지 N회.

| Property | 값 |
|----------|-----|
| `TargetType` | `AoE_Sphere` |
| `AoERadius` | 250 |
| `PeriodicTickCount` | 6 |
| `PeriodicTickInterval` | 0.5 |
| `DamageGameplayEffectClass` | `GE_Damage` |
| `BaseDamage` | 50 (틱당) |
| `bAllowMovementDuringCast` | `false` (회전 시전 중 정지) |
| `CooldownDuration` | 10 |

> `PeriodicTickCount > 0` + `AoE_Sphere` = 주기 펄스. **즉발 1회 AoE 가 필요하면 `PeriodicTickCount=0` 으로 두면 됨** (시전 시점 1회 `OverlapMultiByChannel`).

### 패턴 D — Single 처형 (R: DemacianJustice)

단일 적, missing HP 비례 데미지.

| Property | 값 |
|----------|-----|
| `TargetType` | `SingleEnemy` |
| `DamageGameplayEffectClass` | `GE_Damage` |
| `BaseDamage` | 250 |
| `MissingHpDamageScale` | 0.3 |
| `bAllowMovementDuringCast` | `false` (몽타주 길이 동안 정지) |
| `CooldownDuration` | 90 |

기본 데미지식: `Damage = BaseDamage + (Target.MaxHP − Target.HP) × MissingHpDamageScale`.

타겟은 `TriggerEventData->Target` 우선, 없으면 `AIController->GetCurrentTargetCharacter()` → `FindNearestEnemy()` 순서로 fallback.

---

## 특이 로직: `Calculate Target Damage` override

기본 식(`Base + Missing × Scale`)으로 표현 불가능한 데미지 — 예: 마법 저항 무시, 특정 태그 보유 시 보너스, 자기 HP 비율 기반 — 은 **BP event graph 에서 override**.

### 예 1. 적이 `State.Bleeding` 보유 시 1.5배

`BP_GA_Char_Skill` 열기 → **Event Graph** → **Right-click → Add Event → Calculate Target Damage** (BlueprintNativeEvent override).

```
[Calculate Target Damage (Target Actor)]
   ↓
   Get Ability System Component (Target Actor)
   ↓
   Has Matching Gameplay Tag (State.Bleeding)
   ↓
   ├─ True  → Base × 1.5 → Return
   └─ False → Call Parent Function → Return  (기본식 그대로)
```

> Parent 호출(=`CalculateTargetDamage_Implementation`)을 통해 기본식 재사용.

### 예 2. 자기 체력 30% 미만일 때 데미지 2배

```
[Calculate Target Damage (Target Actor)]
   ↓
   Get Owning Actor From Actor Info → Cast to AOSCharacter
   ↓
   GetCurrentHealth / GetMaxHealth → < 0.3 ?
   ↓
   ├─ True  → Call Parent × 2 → Return
   └─ False → Call Parent       → Return
```

---

## 흔한 함정

### 1. `bAllowMovementDuringCast` 의 진짜 의미

| 값 | StateTree task 수명 | 캐릭터 이동 | ABP 동작 |
|----|-------------------|-----------|---------|
| `true` | 즉시 Succeeded (Design B 재선택) | 그대로 이동 | `bIsMoving=true` → 상체만 스킬 (Layered Blend) |
| `false` | `State.Rooted` 동안 RUNNING 유지 → AI 가 홀드 | `StopMovementImmediately()` 로 정지 | `bIsMoving=false` → 전신 스킬 |

이동 가능 스킬에서도 `State.Casting` 은 부여됨 → ABP 의 상체 슬롯 분리 트리거. UPPER_LOWER_BODY_SPLIT.md 참고.

### 2. `SelfAppliedEffects` vs `TargetAppliedEffects`

| 배열 | 적용 대상 |
|------|---------|
| `SelfAppliedEffects` | caster 자신 (예: 이동속도 버프, 실드, 강화) |
| `TargetAppliedEffects` | SingleEnemy 타겟 또는 AoE 각 적 (예: 슬로우, 스턴, 마크) |

Self 패턴 스킬에서 `TargetAppliedEffects` 채우면 적용 안 됨(TargetType=Self 분기는 skip).

### 3. `DamageGameplayEffectClass` 를 비워두면 데미지 미적용

자기 버프 전용 스킬(Q/W)은 비워두는 게 정상. 적에게 데미지를 줘야 하는 스킬인데 비워뒀다면 — 적용 0 → 디버깅 시 의심 1순위.

### 4. `PeriodicTickCount = 0` + `AoE_Sphere` = 즉발 1회 AoE

타이머 안 걸고 시전 시점 단 1회 overlap. 광역 발화/단발 폭발 패턴에 사용.

### 5. `ExplicitRootDuration` 우선순위

`-1` 일 때 root 길이 결정 순서:
1. `Periodic > 0` → `PeriodicTickCount × PeriodicTickInterval`
2. 아니면 → 몽타주 길이

특수 케이스(타이머는 길게, root 는 짧게) 가 필요할 때만 양수로 설정.

### 6. 같은 `SkillIdentityTag` 를 C++ + BP 양쪽에 부여하지 말 것

기존 `UGA_Alex_Q/W/E/R` 클래스가 아직 살아있는 동안(마이그레이션 3단계 전), `BP_Char_Alex.StartupAbilities` 에 **둘 다 추가하면 이중 활성화**. 마이그레이션 중에는 BP child 하나만 사용.

### 7. 쿨다운 GE 의 태그 그랜트 누락

`BP_GE_Cooldown_*` 에 `Target Tags Gameplay Effect Component` 를 추가하지 않으면:
- 쿨다운은 적용되지만(Duration GE 자체는 active)
- `Cooldown.Skill.*` 태그가 부여되지 않아 StateTree 의 `HasCooldownTag` 가 false → **매 tick 활성화 시도** → 스팸.

쿨다운 GE BP 작성 후 즉시 PIE 로 1회 검증 권장 (`showdebug abilitysystem` → 활성 GE 목록에서 태그 확인).

### 8. `UTargetTagsGameplayEffectComponent` 의 Inherited Tags 위치

UE 5.4+ 패턴에서 컴포넌트의 `Added` 가 아니라 `Removed` 에 실수로 입력하면 태그가 안 부여됨. **반드시 `Added` 칸**.

---

## 마이그레이션 이력

| 단계 | 내용 | 상태 |
|------|------|------|
| 1 | C++ base 작성 (`UGA_SkillBase`, `UGE_SkillCooldown_Base`) | ✅ 완료 |
| 2 | BP child 자산 8개 (Alex Q/W/E/R + 쿨다운 4개) 생성 + `BP_Char_Alex` 매핑 교체 | 🔜 진행 중 (design-balance) |
| 3 | 구 C++ 클래스 (`UGA_Alex_Q/W/E/R`, `UGE_Cooldown_Alex_Q/W/E/R`) 제거 | 🔜 예정 |

### 이전 패턴의 한계

- 캐릭터 1명 추가 = C++ 클래스 8개(Ability 4 + Cooldown 4) 추가 + 풀 리빌드
- 디자이너가 수치 1개 변경에도 프로그래머 + 빌드 의존
- Hot Reload 비호환 영역이 매번 늘어남

### 현 패턴의 효과

- 캐릭터 1명 추가 = BP 자산 8개 + 태그 등록 — 빌드 불필요
- 디자이너 단독 작업 가능 (수치, 쿨다운, GE 조합 모두 BP)
- 특이 데미지식만 BP event graph 의 한 함수 override

---

## 검증 체크리스트

새 스킬 BP 자산 작성 후 PIE 에서 다음 항목 확인:

- [ ] `showdebug abilitysystem` → 캐릭터의 Activatable Abilities 에 `BP_GA_<Char>_<Slot>` 보임
- [ ] StateTree 가 조건 만족 시 ability 활성화 (Gameplay Debugger `'` 키 → StateTree 카테고리)
- [ ] 쿨다운 GE 적용 직후 `Cooldown.Skill.*` 태그 active (showdebug abilitysystem 의 OwnedTags)
- [ ] `CooldownDuration` 만큼 후 태그 해제 → 재활성화 가능
- [ ] 데미지 스킬: 적 HP 변화 (HP 바 + `Health = X → Y` 로그)
- [ ] Self 버프 스킬: `SelfAppliedEffects` 의 GE 가 caster 에 active
- [ ] 시전 중 `State.Casting` 태그 active (ABP 상체 분리 트리거)
- [ ] `bAllowMovementDuringCast = false` 스킬: 시전 동안 `State.Rooted` 부여 + 캐릭터 정지

---

## 관련 문서

- `CLAUDE.md` → "스킬 데이터 주도식 — `UGA_SkillBase`" — C++ 흐름 상세
- `CLAUDE.md` → "StateTree 스킬 통합 — Design B" — 자식 노드 우선순위
- `CLAUDE.md` → "UE 5.4+ GameplayEffect Component 시스템" — `UTargetTagsGameplayEffectComponent` 패턴
- [UPPER_LOWER_BODY_SPLIT.md](./UPPER_LOWER_BODY_SPLIT.md) — `bAllowMovementDuringCast` 의 ABP 측 동작 + 슬롯 배치
- [TIMELINE.md](../05_ProgressLog/TIMELINE.md) — 리팩토링 이력
