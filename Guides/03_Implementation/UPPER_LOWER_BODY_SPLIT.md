# 스킬 상하체 분리 (Layered Animation) + 시전 중 이동 플래그

**작성일**: 2026-05-28
**대상**: `prog-anim` / `art-anim` 에이전트, ABP 작업자
**관련 Phase**: Phase 4+ (스킬 시전 root + 상하체 분리)

---

## 목표

Phase 4 스킬(Q/W/E/R) 시전 중 발생하던 **슬라이드**(StateTree task 즉시 완료 → 이동 재개하는데 DefaultSlot 전신 몽타주가 계속 재생) 해결.

- **이동 중**: 상체만 스킬 애니, 하체는 locomotion (상하체 분리)
- **정지 중**: 전신 스킬 애니
- **스킬별 "시전 중 이동 가능 여부"** 를 GA 플래그로 지정 — 불가 스킬은 AI 가 스킬 끝까지 해당 state 에 홀드

핵심: **스킬 GA 의 플래그 1개(`bAllowMovementDuringCast`)가 모든 것을 결정**하고, 애니는 `bIsMoving` 으로 상하체 분리/전신을 자동 선택.

```
bAllowMovementDuringCast
 ├─ true (Q/W): root 안 함 → StateTree task 즉시 Succeeded → 이동 재개
 │              → 이동 중이면 ABP 가 상하체 분리 (상체 스킬 + 다리 locomotion)
 └─ false (E/R): GA 가 State.Rooted 부여 + StopMovement
                → StateTree task 가 State.Rooted 동안 RUNNING 유지 → AI 가 홀드
                → 캐릭터 정지 → ABP 가 전신 스킬 애니
                → 몽타주/회전 종료 → State.Rooted 해제 → task Succeeded → 이동 재개
```

---

## Part A — C++ (구현 완료)

| 요소 | 파일 | 내용 |
|------|------|------|
| 태그 | `Config/DefaultGameplayTags.ini` | `State.Rooted`, `State.Casting` |
| GE | `GAS/Effects/GE_Rooted.h/cpp` | Duration GE, `SetByCaller(Data.Duration)`, `State.Rooted` 부여 |
| 캐릭터 헬퍼 | `AOSCharacter.h/cpp` | `ApplyCastRoot(float Duration)` — GE_Rooted self 적용 + `StopMovementImmediately()` |
| 스킬 GA ×4 | `GAS/Abilities/GA_Alex_Q/W/E/R` | `bAllowMovementDuringCast` UPROPERTY + 생성자 `State.Casting` + 몽타주 직전 root 호출 |
| StateTree task | `AI/AOSStateTreeTasks.h/cpp` | `FStateTreeTask_ActivateAbilityByTag` — `State.Rooted` 보유 시 RUNNING 유지(EnterState+Tick) |
| AnimInstance | `AOSAnimInstance.cpp` | `bIsCasting` = `State.Casting` 태그 미러 |

**플래그 기본값**:
- **C++ 헤더 default** (`GA_Alex_*.h` 의 UPROPERTY 초기값): Q=true, W=true, E=false, R=false
- **현 BP 운영 값** (`BP_GA_Alex_*` Class Defaults — 2단계 데이터 주도 마이그레이션 후): **Q=false**, W=true, E=false, R=false
  - 변경 사유: "Q 풀 애니메이션 재생" 디자인 결정 → Q 도 R 처럼 root + 홀드 패턴 채택 (이동 버프는 Q 종료 후 효과)
- 디자이너가 BP CDO 에서 자유 조정 가능. 헤더 default 는 3단계 cleanup 에서 기존 C++ 4종이 제거되면 의미 없어짐.

**root 길이**: Q/W = 미적용. E = `MaxSpinTicks * SpinTickInterval`(3s, 회전 지속). R = 몽타주 길이.

> 신규 UCLASS/UPROPERTY/USTRUCT 추가 → **풀 리빌드 필요** (적용 완료).

---

## Part B — ABP 애니메이션 (에디터 수동 작업)

> AnimGraph 노드 배선은 MCP/Python 자동화 불가 → 에디터에서 직접 작업.

### 환경 (점검 확인됨)

| 항목 | 값 |
|------|----|
| ABP | `/Game/AOS/Anim/ABP_AOSCharacter` (vars: `Speed`, `Direction`, `bIsMoving`, `bIsCasting` 등 OK) |
| 스킬 몽타주 | `/Game/AOS/Anim/Montages/AM_Alex_{Q,W,E,R}` (현재 DefaultSlot) |
| 스켈레톤 | `SK_Mannequin_UE4_WithWeapon_Skeleton` (UE4 마네킹) |
| 상체 마스크 기준본 | `spine_01` |

### B-1. UpperBody 슬롯 생성 + 스킬 몽타주 재배치

1. 아무 애님 에디터(예: `AM_Alex_Q`) → **Window → Anim Slot Manager**
2. **Add Slot** → 이름 `UpperBody` (그룹 `DefaultGroup`)
3. `AM_Alex_Q/W/E/R` 각각 열기 → Montage 패널 슬롯 트랙 드롭다운에서 **DefaultSlot → UpperBody** → 저장
   - ⚠️ Attack/Death 몽타주(`AM_Attack`, `AM_Death`)는 **DefaultSlot 유지** (전신용)

### B-2. ABP AnimGraph 배선

⚠️ **AnimGraph 포즈 핀은 "출력 → 입력 1개"만 가능** (데이터 핀과 다름). 한 포즈를 여러 곳에 쓰려면
반드시 `Save Cached Pose` 로 캐시해야 한다. 따라서 `Slot 'UpperBody'` 출력도 캐시('UpperFull')해서
LayeredBlend 와 Blend Poses by bool 양쪽에 `Use cached pose` 로 연결한다.

```
Locomotion(BS)
  → Slot 'DefaultSlot'              ← (기존) Attack/Death 전신, 평소엔 통과
  → [Save Cached Pose]  이름: BasePose

[Use cached 'BasePose'] → Slot 'UpperBody' → [Save Cached Pose]  이름: UpperFull
   (= 스킬 몽타주를 전신에 적용한 포즈를 캐시)

UpperSplit = [Layered blend per bone]
   Base Pose     = Use cached 'BasePose'
   Blend Poses 0 = Use cached 'UpperFull'
   Details → Layer Setup → Branch Filter: Bone = spine_01, Blend Depth = 1
   Details → Mesh Space Rotation Blend = true (권장)
   (= 스킬은 spine_01↑ 상체만, 하체는 BasePose = locomotion)

[Blend Poses by bool]   ← 핵심
   Active Value = bIsMoving
   True  (이동) = UpperSplit (Layered blend 출력)
   False (정지) = Use cached 'UpperFull'
   Blend Time ≈ 0.15s  (팝 방지)
   → Output Pose
```

**동작 결과**
- 스킬 미사용: `UpperFull == UpperSplit == BasePose` → 평소 로코모션/공격 그대로 (영향 없음)
- 스킬 + 정지(E/R, root): **전신 스킬**
- 스킬 + 이동(Q/W): **상체만 스킬 + 다리 로코모션** → 슬라이드 없음

**주의점 / 팁**
- `Slot 'UpperBody'` 노드는 **1개만** 만들고 그 출력을 `Save Cached Pose`('UpperFull')로 캐시 →
  `Use cached pose 'UpperFull'` 노드 **2개**로 LayeredBlend `Blend Poses 0` 와 bool-blend `False Pose` 에 각각 연결.
  (포즈 핀은 한 출력을 두 입력에 직접 못 꽂음 → 캐시 필수. 같은 슬롯 노드 2개도 금지 = 충돌.)
- **Layered blend per bone 의 Layer Setup(spine_01)은 Details 패널에서** 설정 — 그래프엔 안 보임. 안 하면 마스크가 안 먹어 전신이 섞임.
- 기존 `Slot 'DefaultSlot'`(Attack/Death 전신)은 **건드리지 않는다**.
- `spine_01` 마스크가 어색하면 Blend Depth 조정 또는 **Blend Mask**(본별 가중치)로 부드럽게.
- (선택 최적화) 전체를 한 번 더 `Blend Poses by bool(bIsCasting)` 로 감싸 시전 중이 아닐 땐 BasePose 직결 → LayeredBlend 비용 절감.
- 상체 전용 자산 **불필요** — Layered Blend Per Bone 본 마스크가 전신 몽타주의 상체만 추출.
- 작업 후 **Compile + Save**.

---

## 검증 (PIE)

1. **Q** (`bAllowMovementDuringCast=true`): 적에게 달려가며 시전 → 다리는 계속 달리고 상체만 스킬, **슬라이드 없음**, AI 안 멈춤
2. **E/R** (`false`): 시전 시 즉시 정지(`ApplyCastRoot`) → **전신 스킬**, 끝날 때까지 AI 그 자리 홀드, 끝나면 이동 재개
3. **플래그 토글**: BP 에서 E 의 `bAllowMovementDuringCast` 를 true 로 → E 도 이동하며 상체 분리되는지
4. **로그**: `showdebug abilitysystem` 으로 `State.Rooted` / `State.Casting` 부여·해제 타이밍 확인

---

## 관련 문서

- `CLAUDE.md` → "Phase 4+: 스킬 시전 root + 상하체 분리 (Layered Animation)" 섹션 (설계/코드 상세)
- `CLAUDE.md` → "StateTree 스킬 통합 — Design B" (task 와 GA 수명 분리, 슬라이드 원인)
