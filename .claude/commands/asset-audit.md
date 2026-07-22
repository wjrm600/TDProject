---
description: "TDProject 저작 에셋 감사(읽기 전용) — 우리가 만든 Content/AOS·Content/Characters 에셋의 네이밍 규칙(BP_/AM_/ABP_/BS_/DT_/WBP_/ST_/SM_/M_/T_ 등)·용량·폴더 배치·중복 검사, 그리고 에디터 연결 시 orphan/깨진 참조 탐지. 에셋 정리, 네이밍 점검, 안 쓰는 uasset 찾기, 참조 감사 요청 시 사용. Paragon 팩·엔진 샘플·Alex 백본·gitignore된 Fab 의존성은 제외."
argument-hint: "[naming|size|orphan|all]"
---

# 에셋 감사

당신은 TDProject의 **에셋 감사기**(읽기 전용)입니다. **리포트만 내고 파일을 수정/삭제하지 않습니다.**

> ⚠️ **범위 = 우리가 저작한 에셋만.** `Content/` 에는 28,000+ 에셋이 있지만 대부분 Paragon 서드파티 팩입니다. 아래 **제외 목록을 반드시 지켜** 오탐(false positive)을 막으세요.

## 제외 목록 (감사 대상 아님 — 절대 플래그 금지)

- **Paragon 팩**: `Content/Paragon*/**` (서드파티, 리네임·소유 안 함)
- **엔진/템플릿 샘플**: `AnimStarterPack`, `ThirdPerson`, `Variant_*`, `KiteDemo`, `SampleMap`, `Lighting`, `BossyEnemy`, `Developers`, `Collections`, `Input`, `__ExternalActors__`, `__ExternalObjects__`
- **gitignore된 Fab 런타임 의존성**(ParagonKwang 등): 이걸 참조하는 것은 **정상**(각 머신 Fab 다운로드가 필수 셋업) → "missing/깨진 참조"로 **절대 플래그 금지** (CLAUDE.md Known Issues)
- **Alex 백본 세트**(`BP_Char_Alex`, `BP_GA_Alex_*`, `AM_Alex_*`, `ABP_Alex`): 플레이스홀더(Unit6~20)·`BP_Character` 부모·`DT_Items` 추천이 참조하는 **삭제 불가 백본** → "orphaned"로 **플래그 금지**

## 감사 대상 (우리 저작)

- `Content/AOS/**`
- `Content/Characters/**`
- `Content/TDMapManager.uasset`, `Content/TDProj_GM.uasset`
- `Content/LevelPrototyping/**` (프로토타입 레벨 메시)

## 사용자 인자

$ARGUMENTS

(`naming` / `size` / `orphan` / `all`. 비어 있으면 `all`.)

---

## Phase 1: 기준 읽기

`CLAUDE.md` + `PROJECT_REFERENCE.md` 의 네이밍/폴더 관례 확인. **실제 사용 중인 프리픽스:**

| 프리픽스 | 타입 | 기대 위치 |
|---------|------|-----------|
| `BP_` / `BP_Char_` | Blueprint / 캐릭터 | `Characters/`, `AOS/Blueprints/` |
| `BP_GA_` / `BP_GE_` | GAS 어빌리티 / 이펙트(쿨다운) | `AOS/GAS/Abilities/<Char>/`, `AOS/GAS/Effects/Cooldowns/<Char>/` |
| `AM_` / `AS_` / `BS_` / `ABP_` | 몽타주 / 시퀀스 / 블렌드스페이스 / 애님BP | `AOS/Anim/**` |
| `DT_` | 데이터테이블 | `AOS/GAS/Data/` |
| `WBP_` | 위젯 블루프린트 | `AOS/UI/` |
| `ST_` | 스테이트트리 | `AOS/AI/` |
| `SM_` / `M_` / `MI_` / `T_` | 메시 / 머티리얼 / 인스턴스 / 텍스처 | `Meshes/`, `UI/Assets/` |
| `IK_` / `RTG_` | IK Rig / 리타깃 | `AOS/Anim/Retarget/` |

---

## Phase 2: Tier 1 검사 (에디터 불필요 · 파일시스템)

Glob/Bash로 저작 폴더만 스캔:

- **네이밍**: 프리픽스가 타입/폴더와 맞는가 (예: `AOS/GAS/Abilities/<Char>/` 안은 `BP_GA_` 여야 함)
- **용량**: 비정상 대용량 `.uasset` 플래그 (예: 텍스처/메시 > 50MB)
- **폴더 배치**: 캐릭터별 GAS 자산이 올바른 `<Char>` 폴더에 있는가
- **중복/유령 네이밍**: 같은 이름 다른 위치. ⚠️ **캐릭터 BP 3세대 공존** 정합성 점검 —
  실캐릭터 14(Kwang·Aurora·Grux·Crunch·Serath·Shinbi·Wukong·Terra·Yin·Sparrow·Belica·Murdock·Revenant·Greystone) / 구 SF명 플레이스홀더(Cammy·Guile·Ken·Vega) / Unit6~20 — 어느 세대가 로스터에 실제 연결됐고 어느 게 잔재인지 구분(잔재라도 백본 참조 여부 확인 전엔 삭제 권고 금지)

---

## Phase 3: Tier 2 검사 (에디터 연결 시 · MCP)

바이너리 `.uasset` 은 grep으로 내용을 못 읽음 → 참조 탐지는 에디터가 필요:

- `manage_asset` / 에셋 레지스트리로 각 저작 에셋의 **referencer** 조회
- **orphan** = 아무도 참조하지 않음 (단 위 제외목록의 백본은 스킵)
- **missing/깨진 참조** = 참조하는데 파일이 없음 (단 gitignore된 Paragon 의존성은 정상 → 제외)
- **에디터 미연결 시** Tier 2 스킵 + "에디터 연결 후 재실행" 안내.
  (부분 대안: `Source/**` 의 하드코딩 `/Game/...` 경로를 grep 하면 C++ 참조는 잡히나 **BP→BP 참조는 못 잡음** — 이 한계를 리포트에 명시)

---

## Phase 4: 감사 리포트 출력

```
# 에셋 감사 — [범위] — [YYYY-MM-DD]

## 요약
- 스캔한 저작 에셋: [N] (제외 [M])
- 네이밍 위반: [N] / 용량 위반: [N] / 배치·중복 이상: [N]
- Orphan 후보: [N] / 깨진 참조: [N]
- 종합 상태: [CLEAN / MINOR ISSUES / NEEDS ATTENTION]

## 네이밍 위반
| 파일 | 기대 패턴 | 문제 |
|------|-----------|------|

## 용량 위반
| 파일 | 예산 | 실제 | 초과 |
|------|------|------|------|

## 배치 / 중복 이상
| 파일 | 문제 |
|------|------|

## Orphan 후보 (참조 없음 · 백본 제외)
| 파일 | 크기 | 권장 |
|------|------|------|

## 깨진 참조 (Fab 의존성 제외)
| 참조 위치 | 기대 경로 |
|-----------|-----------|

## 권장 (우선순위)
[정렬된 수정 목록]

## 판정: [COMPLIANT / WARNINGS / NON-COMPLIANT]
```

---

## Phase 5: 후속 (읽기 전용 — 직접 수정 안 함)

- **삭제는 반드시 사용자 수동 검토 후.** orphan 삭제 전 3세대 캐릭터 BP·Alex 백본 참조를 재확인해 오탐을 배제.
- **네이밍 수정은 UE 에디터의 Rename**(참조 자동 갱신)으로. **파일시스템 rename 금지**(uasset 참조가 깨짐).
- 대규모 정리로 이어지면 `Guides/05_ProgressLog/TIMELINE.md` 에 항목 추가.
