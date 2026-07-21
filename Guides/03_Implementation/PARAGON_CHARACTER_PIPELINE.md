# Paragon 히어로 편입 파이프라인 — 고유 캐릭터 양산 가이드

**대상**: 이 저장소에서 고유 캐릭터를 추가하는 에이전트/개발자.
**방식(2026-07-18 확정)**: Epic Paragon 히어로 애셋(메시/애니)을 **리타깃 없이 그대로 사용**.
절차적 저작 → IK 리타깃 → **리타깃마저 폐기**의 최종 형태. 14회 실증(로스터 0~13, 근접 10 + 원거리 4 = 벤픽 14 달성).

> 스크립트: [`Mcp_Tools/Asset_Pipeline/build_paragon_character.py`](../../Mcp_Tools/Asset_Pipeline/build_paragon_character.py)
> — `run(config)` 한 번에 BS·ABP·BP복제·로스터·몽타주7·GA/GE8+전배선. 상세 스키마/실행법은 그 파일 docstring.

---

## 0. 왜 네이티브 직접 사용인가

| | 리타깃(구) | Paragon 네이티브(현) |
|---|---|---|
| 메시 | AI생성→AccuRig→리타깃 | Paragon 메시 그대로 |
| 무기 | 소켓 부착 + 축 보정(최대 삽질) | **메시 내장 본**(소켓 0) |
| 애니 | 리타깃 배치(품질 편차) | 원본 그대로(프로덕션 품질) |
| 속도 | 캐릭터당 수시간 | **config + run() = 분 단위** |

대가는 하나: **저장소 비대**. Paragon 팩(1.4~3.0GB)은 커밋 불가 → 각 머신 Fab 재다운로드.

---

## 1. 사전 조건 (캐릭터당 1회)

1. **Fab(Epic 영구무료)에서 Paragon 히어로 팩 다운로드** → `Content/Paragon<Hero>/` 로 프로젝트에 임포트.
   - ⚠️ **런타임 필수 의존성**: 팩이 없으면 `BP_Char_<Hero>` 참조가 깨져 프로젝트가 정상 오픈 안 됨. 각 머신에서 받아야 함(엔진 설치급 셋업).
2. **`.gitignore` 에 즉시 등록**: `Content/Paragon<Hero>/`
   - ⚠️ **함정**: `git check-ignore <dir>/` 는 trailing-slash 로 **오탐**(무시로 착각)한다. 반드시 **`git status --porcelain | grep Paragon<Hero>`** 로 `??` 노출 여부 확인. 노출되면 아직 미등록 → `git add .` 한 번에 수 GB 커밋 사고.

### 🖥️ 새 장치 셋업 (클론 → 바로 실행) — 의존성 스캔으로 검증(2026-07-20)

AOS 게임 맵(`Lvl_MainMenu`/`Lvl_ThirdPerson`)은 **커밋된 프로젝트 콘텐츠 + 아래 Paragon 캐릭터 팩 14개 + 엔진** 에만 의존. 순서:
1. `git clone` (프로젝트 콘텐츠 `/Game/AOS`·`/Game/Characters`·`/Game/BossyEnemy`(char1/Alex 백본·마네킹) 포함) → 2. C++ 빌드(소스 커밋됨) → 3. **Fab 에서 아래 14팩만 다운로드** → 즉시 실행.

**Fab 다운로드 필요 (14팩)**: ParagonKwang · Greystone · Grux · Crunch · Aurora · Serath · Shinbi · SunWukong · Terra · Yin · Sparrow · LtBelica · Murdock · Revenant.
**불필요**(각 팩의 데모/샘플 맵만 참조, AOS 게임 미사용): `AnimStarterPack` · **`ParagonProps`(~12GB)** · `KiteDemo` · `Lighting` · `SampleMap` · ParagonBoris(미편입). → 안 받아도 게임 정상 실행.

---

## 2. 캐릭터당 체크리스트

### ① 조사 (config 근거 확보) — MCP `execute_python`
- **스켈레톤/메시 경로**: `/Game/Paragon<H>/Characters/Heroes/<H>/Meshes/<H>_Skeleton`, `.../<H>`(또는 스킨 메시).
- **무기 본**: 메시 컴포넌트 `get_bone_name` 순회 → `weapon`/`sword`/`shield`/`claw` 등 검색. 있으면 내장(소켓 0), 없으면 맨손(Crunch) 또는 원거리(Boris `ik_hand_gun`).
- **발 높이**: `mesh.get_bounds()` minZ. **발 원점(≈0)이면 Z=-88**. 아니면 -minZ 보정.
- **로코모션 인플레이스**: `Idle`/`Jog_*` 의 root drift 0 확인(Paragon 로코모션은 대부분 인플레이스). **이름 주의**: 히어로마다 `Jog_Left/Right`(Kwang/Greystone) vs `Jog_Lft/Rgt`(Grux) 다름 → config `loco` 로 흡수.
- **스킬 애니 매핑**: `Ability_*` 이름을 안 쓰는 히어로 많음(Grux `DoublePain`/`Stampede`/`Ultimate_Roar`). 로코모션/기본공격/공용을 제외 필터해 **스킬 동작 후보**를 추린 뒤 Q/W/E/R 확정(사용자 승인).

### ② config 작성 (`CONFIG` 스키마 — 스크립트 docstring 참조)
```python
{'name','display_name','pack_anim','skeleton','mesh','roster_index','z_offset',
 'loco':{idle,fwd,bwd,left,right}, 'montages':{Attack,Q,W,E,R,Death,HitReact}}
```
- **`roster_index`**: 로스터 20슬롯 중 **0~10 실캐릭터 채움**(근접 10 + 원거리 Sparrow). 11~19 는 플레이스홀더(Unit12~20). 다음 편입은 인덱스 11부터(원거리 히트스캔 — `'attr_row':'Ranged'` 추가).
- 기존 캐릭터 config 복붙 → 6개 필드(pack/skeleton/mesh/roster_index/loco/montages)만 교체.

### ③ `run(config)` 실행 — MCP `execute_python`
```python
import sys; sys.path.insert(0, r'.../Mcp_Tools/Asset_Pipeline')
import importlib, build_paragon_character as bpc; importlib.reload(bpc)
log = bpc.run(<config>)   # 로그는 파일로 저장해 Read (execute_python 은 예외 시 stdout 버림)
```

### ④ BS 그리드 리빌드 — MCP (유일한 후처리)
`animation_physics force_rebuild_blend_space  /Game/AOS/Anim/BS_<Name>_Locomotion`
(BlendSpace 삼각분할은 Python 네이티브 API 부재)

### ⑤ 에디터 수동 2가지 (사용자)
- **ABP AnimGraph 배선**: `ABP_<Name>` → `BS_<Name>_Locomotion`(BlendSpace **Player**) → `DefaultSlot` → Output. `ABP_Kwang` 복사가 기준.
- **`AM_<Name>_Attack` 3섹션**(크리 활성): B·C(또는 D) 클립 추가 + `AttackA`/`AttackB`/`Crit` 섹션 + Montage Sections 패널 `Clear`(독립 재생). 상세 = 팀 내 레시피(Kwang Attack 항목).

### ⑥ PIE 검증 (Play as Dedicated Server / Listen 2-Client)
로코모션(옆걸음)·기본공격(슬롯 DefaultSlot 매칭)·스킬 Q/W/E/R·크기(Z 오프셋).

### ⑦ 선별 커밋
`git add` 로 `ABP_/BS_/AM_<Name>_*`, `GAS/*/​<Name>/`, `BP_Char_<Name>`, `TDProj_GM`, 스크립트 config, `.gitignore` 만 명시. **Paragon 팩은 절대 스테이징 금지**(②에서 gitignore 확인).

---

## 3. 캐릭터 유형별 변형

| 유형 | 예 | 주의 |
|---|---|---|
| 양손/무기 내장 | Kwang(대검)·Greystone(검+방패)·Grux(야수) | 표준. 소켓 0. 그대로 진행 |
| 맨손 격투 | Crunch(로봇) | 무기본 없음(부착 이슈 자체 無). 공격이 `Ability_Combo_*` 형태 → `montages.Attack` 만 조정 |
| **원거리** | Boris(총) | ⚠️ AOS 기본공격 = **근접 몽타주+즉시 데미지**. 원거리면 "총 모션인데 근접 타격" 부조화. **근접 뭉갬 vs 발사체 시스템(GA+Projectile) 신규** = 설계 결정 선행 |

---

## 4. 공통 함정 (전부 실측)

- **로코모션 strafe 미사용 통일**: 순수 `Jog_Strafe_*` 없는 히어로(Greystone/Grux) 있어 **`Jog_Left/Right`(또는 `Lft/Rgt`)로 통일**. 옆걸음/선회 차이 미미.
- **velocity(점프) off**: `GA_SkillBase.bLaunchOnActivate` 는 검증 방해(캐릭터 튐)로 현재 false. 점프 필요 시 BP CDO 에서 켜기(코드 보존).
- **스킬 슈퍼아머**: 긴 스킬 시전 중 피격 시 HitReact 가 스킬 몽타주 덮어써 끊김 → `Multicast_PlayHitReact` 가 `State.Casting` 중 HitReact 스킵(C++, 모든 캐릭터 공통).
- **스킬 태그 Alex.\* 재사용**: GA 에 몽타주 참조 없음(Character `SkillMontages` TMap 경유). 태그는 슬롯 식별용이라 재사용 무해.
- **키 편차**: Paragon 히어로는 195~260cm 로 큼. Z=-88 시작 후 PIE 미세조정.
- **⚠️ additive idle 함정**: 일부 히어로의 `Idle` 은 **additive 애니**(`additive_anim_type != AAT_NONE`, 예: Belica `Idle`=`AAT_LOCAL_SPACE_BASE`). 이를 BS 일반 샘플로 쓰면 가산 델타가 절대 포즈에 적용돼 **메시 스케일 왜곡**(Speed 0 에서 쪼그라들고 속도↑ 시 원복). → **idle 은 반드시 `AAT_NONE` 클립**(비가산 `Idle_Relaxed` 등) 사용. 신규 편입 시 `load_asset(idle).get_editor_property('additive_anim_type')` 확인. 수정은 BS **삭제 없이 in-place**(speed 0 샘플 animation 교체 → `force_rebuild_blend_space`)로 해야 ABP 배선 보존.
- **MCP 함정**: `create_montage`=껍데기(`AnimMontageFactory.source_animation` 우회) · `set_axis_settings`=반영 안 됨(`blend_parameters[i]` 직접 mutate) · `create_animation_blueprint` parentClass 무시(스크립트는 `AnimBlueprintFactory.parent_class` 로 네이티브 생성) · `slot_anim_tracks`/`sample_data` 접근 제한.

---

## 5. 진행 현황 (실캐릭터 14종 — **벤픽 14 달성**)

벤픽 드래프트 14스텝(밴4+픽10) 전부 고유 소비 → 최소 14 실캐릭터 필요 → **충족**.

| # | 캐릭터 | 무기 | 상태 |
|---|--------|------|------|
| 0~4 | Kwang·Greystone·Grux·Crunch·Aurora | 근접 | ✅ |
| 5~9 | Serath·Shinbi·SunWukong(Wukong)·Terra·Yin | 근접 | ✅ (Wukong·Terra R=Cast 대체) |
| 10 | Sparrow | 궁수(**원거리**) | ✅ 히트스캔 1호 |
| 11 | LtBelica(내부 Belica) | 캐논(**원거리**) | ✅ (idle=HeroSelect_Idle, additive 회피) |
| 12 | Murdock | 총(**원거리**) | ✅ |
| 13 | Revenant | 쌍권총(**원거리**) | ✅ |

**근접 10 + 원거리 4 = 실캐릭터 14.** 인덱스 14~19 는 플레이스홀더(Unit15~20). 미편입 원거리 = Boris 1종만 `.gitignore` 잔존(로스터 확장 시 동일 방식).

### 🎯 원거리 = 히트스캔 (발사체 시스템 불필요 — 검증 완료)

데미지는 **위치가 아니라 타겟에 직접 적용**(`GA_Attack` 거리/트레이스 체크 없음). AI 는 `GetEffectiveAttackRange()`(=`AttackRange` 속성)에서 멈춰 공격. 따라서:
- **원거리 유닛 = `AttackRange` 큰 스탯 행 + 발사(fire) 애니.** C++/리빌드 0.
- `DT_CharacterAttributes` 에 **`Ranged` 행(AttackRange 900, MoveSpeed 550)** 신설. 원거리 BP 는 `AttributeInitRowName='Ranged'` → 스크립트 config `'attr_row':'Ranged'` 로 자동 설정.
- config `Attack` 를 `Primary_Fire_*` 로 매핑. Sparrow(궁수) 로 "멀리서 정지→즉시 명중" PIE 검증 완료.
- 날아가는 투사체는 순수 VFX(선택적 후속). ⚠️ 밸런스: 현재 근접은 `Alex` 행(AP1/AS2) 공유, `Ranged`는 AP10 → 유닛 밸런스 미조정(별도 패스).

### 편입 대기 팩 (Fab 임포트 완료, 전부 `.gitignore` — 커밋 금지)

| 유형 | 팩 (⚠️=내부 폴더명 불일치) | 상태 |
|---|---|---|
| 원거리 (히트스캔) | Boris | 로스터 확장 시 Sparrow 방식(`attr_row='Ranged'`) 그대로 편입 |
| 비캐릭터 | ParagonProps (~12GB) | 환경/소품 — 편입 대상 아님 |

> ⚠️ **양산 병목 = 캐릭터당 에디터 수동 2가지**(ABP AnimGraph 배선 + Attack 3섹션). 스크립트 파트(config+run)는 분 단위지만 이 수동은 자동화 미해결 — 대량 확장 시 이게 실제 비용. (스킬 매핑 오류는 몽타주만 재생성하면 되고 ABP 배선과 독립이라 배선 작업은 보존됨.)

> 관련: [`build_paragon_character.py`](../../Mcp_Tools/Asset_Pipeline/build_paragon_character.py) · CLAUDE.md "캐릭터 로스터"/"애니메이션" · [`TIMELINE.md`](../05_ProgressLog/TIMELINE.md) 2026-07-18 항목들 · [`AI_3D_ASSET_PIPELINE.md`](AI_3D_ASSET_PIPELINE.md)(구 리타깃/Meshy 방식, historical).
