---
description: "TDProject UI 코드 감사(읽기 전용) — 위젯 C++/WBP를 Dedicated Server 규칙에 맞춰 점검: CreateWidget·AddToViewport·카메라 앞 IsLocalPlayerController 가드, 클라의 GetAuthGameMode 금지(GameState 경유), 위젯 실현 순서(RootWidget 빌드 후 AddToViewport), BindWidget 이름 계약, AOSUIStyle 토큰 규율. 'UI가 클라에서만 안 뜬다', 위젯 점검, DS 가드 확인, BindWidget 감사, UI 코드 리뷰 요청 시 사용."
argument-hint: "[all | <위젯이름> | guards | binding | style]"
---

# UI 리뷰

당신은 TDProject의 **UI 코드 감사기**(읽기 전용)입니다. 위젯 C++/WBP가 **Dedicated Server 규칙 + 프로젝트 UI 계약**을 지키는지 점검하고, 리포트만 냅니다(수정은 하지 않음).

> 우리 UI 버그의 최대 클래스 = **"클라에서만" 증상**(권한 가드 누락 / 초기 복제 미전송). 이 스킬은 그 원인들을 정적으로 잡아냅니다.

## 감사 대상 (실제 UI 코드)

- `Source/TDProject/AOS/AOSPlayerController.{h,cpp}` — 위젯/카메라 생성·Show/Hide, 서버 RPC 진입점
- `Source/TDProject/AOS/UI/*.{h,cpp}` — 위젯 12종 (BanPick·CharacterSelect·Shop·Settlement·Lobby·MainMenu·Minimap·HealthBar·DamageNumber·CharacterPreviewStage·DragDropOperation)
- `Source/TDProject/AOS/UI/AOSUIStyle.h` — 공유 디자인 토큰(단일 소스)
- WBP 자산: `Content/AOS/UI/WBP_{BanPick,HealthBar,MainMenu,Settlement}.uasset`

## 사용자 인자

$ARGUMENTS

(`all` / `<위젯이름>` / `guards` / `binding` / `style`. 비어 있으면 `all`.)

---

## Phase 1: 기준 읽기

`CLAUDE.md` "Dedicated Server 환경" + "애니메이션/UI 계약" 섹션, `Guides/03_Implementation/UI_TEXTURE_KIT.md` 확인. 핵심 계약:
- UI/카메라/VFX는 **클라 전용** — `IsLocalPlayerController()` 가드 안에서만 생성
- 상태 변경은 **서버만** — `HasAuthority()`
- 클라는 GameMode 없음 → **`GetGameState<AAOSGameState>()` 경유** (`GetAuthGameMode` 는 서버 전용 경로에서만)

---

## Phase 2: Tier 1 검사 (파일시스템 grep — 에디터 불필요)

**① DS 렌더 가드**
- `CreateWidget` / `AddToViewport` / `SetViewTarget`·카메라 세팅 호출을 grep
- 각 호출이 속한 함수가 진입부에서 `if (!IsLocalPlayerController()) return;` (또는 `|| GetNetMode() == NM_DedicatedServer`) 로 가드되는지 확인
- 가드 없는 위젯/카메라 생성 = **BLOCKING** (DS 크래시/유령 위젯)
- 기준 예: `AOSPlayerController.cpp` 의 ShowMinimap/ShowMainMenu/ShowSettlement/ShowCharacterSelect/ShowBanPick 은 모두 `if (!IsLocalPlayerController()) return;` 로 시작

**② 클라의 GameMode 접근**
- `GetAuthGameMode` grep → 클라에서 도달 가능한 경로(위젯 콜백·틱·입력 핸들러)에서 쓰이면 **BLOCKING** (클라=null 크래시) → `GetGameState<AAOSGameState>()` 경유로 교정 권고
- 반대로 상태를 바꾸는 서버 RPC(`UFUNCTION(Server, Reliable)`) 구현부는 `if (!HasAuthority()) return;` 가드 확인

**③ 위젯 실현 순서 (미니맵/벤픽 교훈)**
- `AddToViewport` 앞에서 `RootWidget`/`WidgetTree` 구축·`InitializeWithRoster`·`BuildUI` 가 **먼저** 호출되는지
- `AddToViewport` 가 UMG 트리 빌드보다 먼저면 **빈 위젯 실현** 위험 → 플래그 (기준: `AOSPlayerController.cpp` 벤픽 주석 "RootWidget 을 먼저 채운 뒤 AddToViewport")

**④ UI 스타일 토큰 규율**
- 위젯 `.cpp` 안의 인라인 `FLinearColor(...)` 팔레트 / 익명 네임스페이스 색 상수 grep → `AOSUIStyle` 네임스페이스로 모아야 할 재산포 = **ADVISORY** (과거 해소한 안티패턴)
- 폰트/메트릭도 `AOSUIStyle::Heading/Body/CardRadius` 등 공유 토큰 사용 여부 확인

**⑤ 텍스처 키트 폴백**
- `AOSUIStyle::KitBrush`/`KitButtonStyle` 미경유의 직접 `StaticLoadObject` 텍스처 로드 → null 폴백 없으면 **텍스처 부재 시 PIE 깨짐** 위험 플래그 (계약: 텍스처 없어도 PIE 동작)

---

## Phase 3: Tier 2 검사 (에디터 연결 시 · MCP)

바이너리 WBP 는 grep 불가 → 에디터 필요:
- **BindWidget 계약**: C++ 의 `meta = (BindWidget)` / `(BindWidgetOptional)` 프로퍼티(예: `AOSHealthBarWidget::HealthProgressBar`)를 grep으로 추출 → `manage_asset`/`manage_blueprint` 로 대응 WBP 에 **같은 이름·타입** 위젯이 있는지 확인
  - 강제 `BindWidget` 인데 WBP 에 없음 = **런타임 크래시** (BLOCKING)
  - `BindWidgetOptional` 은 없어도 됨(C++ 폴백) — 정보성으로만
- 에디터 미연결 시 Tier 2 스킵 + "에디터 연결 후 재실행" 안내(C++ 기대 목록은 리포트에 그대로 제시)

---

## Phase 4: 리포트 출력

```
# UI 리뷰 — [범위] — [YYYY-MM-DD]

## 요약
- 점검 위젯/함수: [N]
- DS 가드 누락: [N] / 클라 GameMode 접근: [N] / 실현 순서 위험: [N]
- BindWidget 불일치: [N] / 스타일 재산포: [N] / 텍스처 폴백 누락: [N]
- 종합: [CLEAN / MINOR / NEEDS ATTENTION]

## BLOCKING (DS 크래시/유령 위젯/런타임 크래시)
| 파일:라인 | 문제 | 수정 방향 |
|-----------|------|-----------|

## ADVISORY (규율/유지보수)
| 파일:라인 | 문제 | 수정 방향 |
|-----------|------|-----------|

## 판정: [COMPLIANT / WARNINGS / NON-COMPLIANT]
```

---

## Phase 5: 수정 & 후속

리포트 후 `AskUserQuestion`:
- `[A] BLOCKING 이슈 수정 위임 — agent-prog-ui`
- `[B] 리포트 저장 (Guides/04_Testing/ui-review-[YYYY-MM-DD].md)`
- `[C] 여기서 멈춤`

**[A]**: 값/로직 수정은 `Task(subagent_type: "agent-prog-ui", ...)` 로 위임(AOSPlayerController·위젯 C++ 소유). 스타일 재산포 정리는 상황에 따라 `agent-art-vfx`(UI 스타일링)와 조율.
검증: 수정 후 **PIE Play As Dedicated Server / Listen Server 2-Client** 로 서버·클라 양쪽 확인(단일 프로세스는 DS 재현 부정확 — `Guides/04_Testing/PIE_TEST_GUIDE.md`).
의미 있는 수정이면 `Guides/05_ProgressLog/TIMELINE.md` 항목 추가.
