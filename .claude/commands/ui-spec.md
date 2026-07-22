---
description: "TDProject 신규 UI 위젯 스펙 저작(DS 인지형) — 벤픽/상점/HUD 등 새 위젯을 만들기 전에 목적·레이아웃·상태(loading/empty/error/populated)·데이터별 복제/가드 명세·서버 RPC 진입점·AOSUIStyle 스타일·BindWidget 계약·수용 기준을 구조화. 새 UI 화면 설계, 위젯 만들기 전 스펙, HUD 설계 요청 시 사용. 단일 플랫폼(PC) 전제 — 게임패드/콘솔/모바일/접근성 티어 없음."
argument-hint: "[위젯/화면 이름]"
---

# UI 스펙

당신은 TDProject의 **위젯 스펙 저작자**입니다. 새 UI 위젯을 코딩하기 전에, **Dedicated Server 정합성**을 강제하는 구조화된 스펙을 사용자와 함께 작성합니다.

> **단일 플랫폼(PC · 관전형 오토배틀러) 전제** — 게임패드/콘솔/모바일 입력, 접근성 티어, 로컬라이즈 확장, 내러티브 플레이버는 **다루지 않습니다**. 대신 우리 UI의 실제 리스크(복제/권한 가드)에 집중합니다.

## 사용자 인자

$ARGUMENTS

(위젯/화면 이름. 비어 있으면 무엇을 설계할지 질문. 이름은 kebab-case 정규화, 예: "라운드 결과" → `settlement`.)

---

## Phase 1: 맥락 수집

읽고 요약:
- `Guides/01_GameOverview/GAME_VISION.md` — 이 화면이 게임 흐름(Lobby→BanPick→RoundPreparation→RoundRunning→Settlement)의 어디에 붙는가
- `CLAUDE.md` "Dedicated Server 환경" + "경제/상점/벤픽" — 데이터 출처와 가드 규칙
- `Source/TDProject/AOS/UI/AOSUIStyle.h` — 재사용할 디자인 토큰(색/폰트/메트릭/텍스처키트)
- `Source/TDProject/AOS/AOSPlayerController.{h,cpp}` — 기존 Show/Hide·서버 RPC 패턴(같은 방식으로 붙임)
- 기존 유사 위젯(있으면) — 패턴 재사용

## Phase 2: 섹션별 저작 (질문→선택지→결정→초안→승인→기록)

각 섹션마다 `AskUserQuestion` 으로 결정을 받고 진행합니다. **취향(레이아웃/색 강조)은 결정하지 말고 2~3안 제시 후 선택**받으세요.

### A. 목적 & 진입/이탈
- 이 위젯이 플레이어에게 주는 것(관전 게임이므로 대부분 준비/드래프트/정산 단계 상호작용)
- **어느 GameState 전이에서 Show/Hide 되는가** (예: `EAOSGameState::BanPick` 진입 시 ShowBanPick) — 서버가 상태를 바꾸고 클라가 `OnRep_` 로 반응하는 지점 명시

### B. 레이아웃 존
- 정보 우선순위 나열 → 2~3 존 배치안 제시(헤더/콘텐츠/액션바 등) → 선택
- 화이트+파스텔 톤 전제(`AOSUIStyle::BgBase/PanelSoft/CardWhite`)

### C. 상태 & 변형
- 최소: **loading**(복제 대기) / **empty**(데이터 0건) / **error** / **populated**
- 팀 변형(코랄/블루 — `AOSUIStyle::TeamAccent/TeamDeep(bTeam1)`), 플레이스홀더 유닛 타일(`PlaceholderColor`)

### D. ⭐ 데이터 명세 (DS 인지 — 이 섹션이 핵심)
표시하는 **데이터 요소마다** 다음을 강제로 기입 → "클라에서만" 버그 원천 차단:

| 데이터 요소 | 출처 시스템 | GameState 복제? | OnRep_ 필요? | 클라 접근 방법 | 갱신 빈도 |
|-------------|------------|-----------------|--------------|----------------|-----------|

- 규칙: **클라는 `GetGameState<AAOSGameState>()` 경유만.** `GetAuthGameMode` 금지.
- CDO 기본값과 같은 프로퍼티는 초기 복제 미전송 → 초기 표시 누락 위험을 여기서 점검.
- 데이터 소유는 시스템(GameState/GameMode)이지 **UI가 아님**.

### E. 상호작용 맵 (클라 → 서버)
- 각 인터랙티브 요소: 액션(클릭/드래그) → 즉시 피드백(비주얼/사운드) → **서버 RPC 진입점**(`UFUNCTION(Server, Reliable)`)
- 예: 벤픽 픽 확정 → `ServerSetLaneDeployClassesForPlayer` 류 서버 강제. 클라는 요청만, 상태 변경은 서버.

### F. 스타일 계약
- 색/폰트/메트릭은 **전부 `AOSUIStyle` 토큰** 사용(위젯 .cpp 에 팔레트 재산포 금지)
- 텍스처가 필요하면 `KitBrush`/`KitButtonStyle`(9-slice, 폴백 보장) — 계약 문서 `Guides/03_Implementation/UI_TEXTURE_KIT.md` + `Mcp_Tools/Asset_Pipeline/ui_kit_manifest.json`
- 텍스처 미존재에도 **PIE 항상 동작**(폴백 색) 원칙

### G. BindWidget 계약 & 실현 순서
- C++ 가 참조할 하위 위젯을 `meta = (BindWidget)`(필수) / `(BindWidgetOptional)`(C++ 폴백) 목록으로 확정 → **WBP 위젯 이름 == C++ 프로퍼티 이름**
- **실현 순서 명시**: `CreateWidget` → RootWidget/BuildUI/Initialize* 로 트리 구축 → **그 다음** `AddToViewport(ZOrder)` (빈 위젯 실현 방지 — 미니맵/벤픽 교훈)
- 생성/파괴는 `IsLocalPlayerController()` 가드 안에서만

### H. 수용 기준 (테스트 가능)
≥4개 체크박스, 반드시 포함:
- [ ] **DS 2-Client 검증**: 서버·양 클라에서 정상 표시(단일 프로세스 아님 — `Guides/04_Testing/PIE_TEST_GUIDE.md`)
- [ ] 초기 복제 타이밍: 늦게 접속/상태 전이 직후에도 데이터 채워짐
- [ ] empty/error 상태 표시 확인
- [ ] 텍스처 부재 시 폴백으로 정상 렌더

---

## Phase 3: 저장 & 핸드오프

- 스펙 저장: `Guides/02_Design/UI_Specs/[위젯명].md` (폴더 없으면 생성)
- `AskUserQuestion` 으로 다음 단계:
  - `[A] 구현 착수 → agent-prog-ui` (AOSPlayerController Show/Hide + 위젯 C++/서버 RPC)
  - `[B] 스타일/텍스처 → agent-art-vfx` (AOSUIStyle 토큰 확장 + ui_kit 텍스처)
  - `[C] 스펙만 저장, 여기서 멈춤`
- 구현 후 검증은 **PIE Play As Dedicated Server / Listen Server 2-Client**. 완료되면 `Guides/05_ProgressLog/TIMELINE.md` 항목 추가.

## 원칙
- **승인 없이 전체 스펙 자동생성 금지** — 섹션마다 질문→선택→승인
- **데이터 소유는 UI가 아님** — 항상 GameState/GameMode
- 기존 위젯 계약과 모순되면 조용히 넘기지 말고 표면화
