# 애니메이션 제작 기획 템플릿 (Paragon 애셋 활용형)

> **방향 전환 (2026-07-04)**: 처음부터(from-scratch) 절차적 저작은 품질 한계가 명확해 폐기.
> 이제는 **Epic Paragon 애셋(현재 `ParagonKwang` = 대검 히어로)을 베이스로 리타깃**해 쓰고,
> **캐릭터 정체성에 필요한 부분만 조금씩 수정**한다(무기 파지·시그니처 포즈·타이밍).
>
> **목적**: 사용자가 "이 파라곤 모션을, 이렇게 바꿔서" 를 적으면 Claude 가
> (파라곤 소스 선택 → IK 리타깃 → 캐릭터 조정 → 루트모션/타이밍/접지 → UE 몽타주 + AnimNotify)
> 로 제작한다.
>
> **사용법**: 아래 [Paragon 소스 카탈로그](#paragon-소스-카탈로그-kwang-대검)에서 베이스를 고르고,
> [블랭크 템플릿](#블랭크-템플릿-복사해서-작성)을 모션 1개당 하나 복사해 채운다.
> **핵심은 "무엇을 바꿀지"** — 소스가 이미 프로 mocap 이라 대부분 그대로 좋고, 조정 포인트만 명확히 적으면 된다.

---

## 스켈레톤 호환성 (2026-07-04 실측 — 왜 리타깃이 필요한가)

| 스켈레톤 | 용도 | 본 수 |
|----------|------|------|
| `SK_Mannequin_UE4_WithWeapon_Skeleton` | **로코모션 ABP 베이스** (Idle/Move/8방향) | UE4 마네킹 |
| `char1_accurig_Skeleton` | **스킬 몽타주 재생**(메시 스켈레톤, Q/W/E/R/Death) | 117 |
| `Kwang_Skeleton` (ParagonKwang) | **소스** (리타깃 원본) | 115 |

- **Kwang ↔ char1 공통 60본** = 전신 포즈 구동 코어 전부(pelvis·spine_01~03·neck·clavicle·upperarm·lowerarm·hand·thigh·calf·foot·ball·손가락01~03). → **몸 동작은 완전 전사.**
- 나머지(char1 `cc_base_*` 트위스트/얼굴/발가락·metacarpal·spine_04/05, Kwang 얼굴/머리/천)는 **보정·장식 본** → 몸 동작 무관.
- rest orientation Y축 dot: 코어 ≥0.96~0.98(양호), **발·손가락 0.83~0.93**(경미 오프셋 → 접지/파지 튜닝 대상).
- **셋 다 UE 마네킹 혈통** → **UE IK Retargeter** 로 프로덕션 품질 전사. (절차적 손계산 리타깃 ❌ — 큰 회전에서 붕괴. 검증된 Blender 회전복사도 발/손가락 오프셋 있어 보조로만.)

**⚠️ "그대로(as-is) 100%" 는 불가** — 스켈레톤이 달라 **리타깃 1스텝은 항상 필요**. 단 그 1스텝이 쉬워졌다는 뜻.

---

## Paragon 소스 카탈로그 (Kwang = 대검)

> 경로: `Content/ParagonKwang/Characters/Heroes/Kwang/Animations/`. `_MSA`/`_Montage`/`_Recovery` 접미사는 변형본.

### 로코모션 (8방향 스트레이프 — 포커스 고정, AimOffset 불필요)
| 방향 | 소스 애님 |
|------|-----------|
| 전진 | `Jog_Fwd` (+ `Jog_Fwd_Start`/`Jog_Fwd_Stop`/`Jog_Fwd_Pivot180`) |
| 후진 | `Jog_Bwd` (+ Start/Stop/Pivot) |
| 좌 (게걸음) | `Jog_Left` / `Jog_Strafe_Left` |
| 우 (게걸음) | `Jog_Right` / `Jog_Strafe_Right` |
| 대각/선회 | `Jog_Fwd_CircleLeft/Right`, `Jog_Bwd_CircleLeft/Right` (블렌드 보간으로 대체 가능) |
| 대기 | `Idle` (+ `Idle_Noise` 미세 흔들림, `Idle_Additive`) |
| 제자리 회전 | `Turn_Left_90/180`, `Turn_Right_90/180` (선택 — 라인전엔 불필요할 수 있음) |

### 공격/스킬 (대검 콤보 + 어빌리티)
| 소스 | 동작 요지 | Alex 슬롯 후보 |
|------|-----------|----------------|
| `PrimaryAttack_A_Slow` | 대검 기본 베기 1타 | 기본 공격 |
| `PrimaryAttack_B_Slow` | 기본 베기 2타 | 기본 공격 변형 |
| `PrimaryAttack_C_Slow` | 오버헤드 → 우측 수평 베기 | **E(수평 스윕)** or 기본 |
| `PrimaryAttack_D_Slow` | 오버헤드 → 수평 콤보 | **E** or **Q** |
| `Ability_RMB` | 오버헤드 슬램 | **R(처형 슬램)** or **Q** |
| `Ability_R` (+`_Intro`) | 도약 + 오버헤드 + 스윕 (궁극기) | **R(도약 처형)** |
| `Ability_Q_Throw`/`Catch` | 검 던지고 받기 | (알렉스 킷과 불일치 — 보류) |
| `Cast` / `Emote_RallyUp` | 시전/포효 자세 | **W(셀프 버프)** |

### 사망 / 피격
| 소스 | 용도 |
|------|------|
| `Death_Bwd` | 사망 (뒤로 쓰러짐 — 전방 크럼플 원하면 미러/역재생 조정) |
| `Hitreact_Fwd/Bwd/Left/Right` | 히트리액트 4방향 |

---

## 블랭크 템플릿 (복사해서 작성)

```
## [모션 이름]                       예) Alex E — Judgment

### 1. 파라곤 소스 선택 ⭐ (제일 중요 — 위 카탈로그에서)
- 베이스 소스:   ParagonKwang/.../__________          (예: PrimaryAttack_D_Slow)
- 왜 이 소스:    ___                                   (원하는 "느낌"과 어디가 맞나)
- (모르겠으면)   원하는 동작만 서술 → Claude 가 카탈로그에서 후보 렌더해 고르게 함

### 2. 기본 정보
- 종류:        [ ] 스킬(1회)  [ ] 루프(Idle·Move)  [ ] 사망(1회)  [ ] 기본공격
- 식별 태그:    Ability.Skill.Alex.__      (스킬만)
- 대략 길이:    약 ___ 초 / ___ 프레임      (소스 길이 유지가 기본 — 늘/줄일 때만 적기)
- 무기:        [ ] 대검(오른손 weapon_r 소켓)  [ ] 무기 없음  [ ] 기타: ___
- 위치 이동:
      [ ] 소스 루트모션 그대로
      [ ] 완전 제자리로 고정 (루트모션 OFF)
      [ ] 도약/전진 거리 조정: ___

### 3. 캐릭터 조정 포인트 ⭐ (소스에서 "바꿀 것"만 — 없으면 비움)
- 무기 파지:    [ ] Kwang 은 한손검, Alex 대검은 양손/오른손 — 파지 본 조정 필요
- 시그니처 포즈: ___                       (예: "마무리에서 검을 땅에 끌기", "더 크게 휘두르기")
- 타이밍 조정:  ___                        (예: "임팩트를 0.2s 앞당겨", "윈드업 홀드 추가")
- 접지/발:      ___                        (발 오프셋 보이면 여기; 기본은 Claude 가 IK 보정)
- 그 외:        ___

### 4. 타격/판정 타이밍 (스킬/공격만)
- 데미지 발생 시점: ___     (예: "검이 수평으로 지나가는 순간", "슬램 착지")
- 범위: [ ] 단일  [ ] 전방 부채꼴  [ ] 360° AoE  [ ] 기타: ___
- 다단 히트: [ ] 1회  [ ] ___회

### 5. 루프 모션만 (Idle·Move)
- Move 방향: [ ] 전진만  [ ] 8방향 스트레이프(포커스 고정 — 옆/뒷걸음)
- Move 속도대: [ ] 걷기  [ ] 달리기  (MoveSpeed=600)
- AimOffset(얼굴/몸통 조준 회전): [ ] 불필요(기본)  [ ] 필요

### 6. 참고 자료 / 기타
- 참고 영상·이미지: ___
- 그 외: ___
```

---

## 작성 팁

| 적는 법 | 좋음/나쁨 | 이유 |
|---------|:---------:|------|
| "PrimaryAttack_D 를 베이스로, 마무리에 검 끌기 추가" | ✅✅ | 소스+조정이 명확 → 한 번에 |
| "이런 느낌인데 어느 소스가 맞을지 모르겠음" + 설명 | ✅ | Claude 가 카탈로그 후보 렌더 → 사용자 픽 |
| "멋있게 휘둘러" | ❌ | 소스도 조정도 없음 → 되물어봐야 함 |
| 참고 영상 링크 / 스크린샷 | ✅✅ | 소스 선택·조정 방향을 눈으로 확정 |

**참고 자료 제출**: 스크린샷은 `Guides/05_ProgressLog/images/<날짜_주제>/` 에 넣어달라고 하면 폴더를 먼저 만들어 드립니다.

---

## 제작 파이프라인 (Claude 자동 처리 — 사용자는 1·3번만 채우면 됨)

1. **소스 선택** → 파라곤 애님 확인(필요 시 후보 콘택트 시트 렌더해 사용자 픽).
2. **IK 리타깃** → Kwang → 타깃 스켈레톤(로코모션=마네킹 / 스킬=char1). UE IK Retargeter 우선.
3. **캐릭터 조정** → 무기 파지(weapon_r 소켓)·시그니처 포즈·타이밍을 Blender 에서 레이어링(80/20, 전체 재작성 X).
4. **자가 시각검증** → `render_skin_preview.py --blend <anim.blend>` 로 **실제 스킨메시** 콘택트 시트 판독(본 프록시 아님). `render_anim_preview.py --qa` 로 팝핑/슬라이드 수치 QA.
5. **재임포트** → `/Game/AOS/Anim/Sequences/AS_Alex_<모션>` (로코모션은 마네킹, 스킬은 char1 스켈레톤).
6. **몽타주/노티파이** → `AM_Alex_<슬롯>` 배치 + `AOSAnimNotify_AttackHit` 를 타격 프레임에 + `SkillMontages` TMap 할당.
7. **루트/이동 정합** → 시전 중 이동불가 스킬 `bAllowMovementDuringCast=false`. 루트모션 모션은 `State.Rooted` 중 하체 블렌드 억제(AOSAnimInstance 처리됨).
8. **저장/커밋** → MCP 변경 즉시 `save_asset`. 커밋·푸시는 요청 시.

> **8방향 로코모션 구현 노트**: `UAOSAnimInstance` 는 이미 `Speed`(cm/s)·`Direction`(-180~180°)·`bIsMoving` 를 계산한다(스트레이프 블렌드스페이스 설계). 구현 = Kwang `Jog_Fwd/Bwd/Left/Right(+Strafe)` 를 마네킹으로 리타깃 → **2D 블렌드스페이스(X=Direction, Y=Speed)** 작성 → ABP 의 단일 `RunAnim` 노드를 이 블렌드스페이스로 교체. 포커스 고정 스트레이프라 AimOffset 불필요.

---

## 기존 완료작 (스크래치/리타깃 혼합 시절 — 참고 기록)

> 아래는 방향 전환 前 제작본. 재작업 시 위 파라곤 파이프라인으로 대체 검토.

- **✅ Alex Q — Decisive Strike** (`AS_Alex_Q`, 커밋 `2dcfddd`): SoulCalibur6 Siegfried 기반 충전코일→오버헤드→전진 런지 슬램. `SwingAndSlam.fbx` swing#2 회전전용 리타깃+우측 파지 미러. 제자리(루트모션 OFF), 40f/1.33s. "확정".
- **✅ Alex R — Demacian Justice** (`AS_Alex_R`): 머리 위 치켜듦→도약→정수리 수직 슬램, 착지=임팩트. `SwingAndSlam.fbx` 다운스윙 바닥을 착지 프레임 정렬, 루트모션 ON. 45f/1.5s.
- **✅ Alex Move** (`AS_Alex_Move`, 마네킹): Boss_Run 하체 + Idle 검-드래그 상체 오버레이. 전진만·달리기·제자리. 27f/0.9s. → **8방향으로 업그레이드 예정(위 노트).**
- **✅ Alex Death** (`AS_Alex_Death`): AnimStarterPack Death_3 전방 크럼플 리타깃. 59f/1.93s. 끝에서 래그돌 인계.
- **⚠️ Alex E — Judgment**: 절차적 360° 스핀 반복 실패 → **파라곤 소스로 재제작 대상**(`PrimaryAttack_C/D` 수평 스윕 유력).
- **⚠️ Alex W — Courage**: 셀프 버프 자세 미완 → `Cast`/`Emote_RallyUp` 소스 후보.

> 관련 문서: [SKILL_AUTHORING_GUIDE.md](SKILL_AUTHORING_GUIDE.md) · [UPPER_LOWER_BODY_SPLIT.md](UPPER_LOWER_BODY_SPLIT.md) · [AI_3D_ASSET_PIPELINE.md](AI_3D_ASSET_PIPELINE.md)
> Anim 자가검증 루프: [Mcp_Tools/Anim_Pipeline/README.md](../../Mcp_Tools/Anim_Pipeline/README.md)
