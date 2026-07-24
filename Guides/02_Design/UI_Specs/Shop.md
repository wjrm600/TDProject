# UI 스펙 — Shop (상점 팝업)

**위젯**: `WBP_Shop` · **Parent C++**: `UAOSShopWidget`
**진입/이탈**: `CharacterSelect`(라운드 준비 화면)의 `ShopButton` 클릭으로 **자식 오버레이**로 열림(별도 화면 아님). 닫기/뒤로로 Collapsed.
**스타일 계약**: 다크·프리미엄 v2, `AOSUIStyle` 토큰. 상세 → [`Guides/02_Design/ART_DIRECTION.md`](../ART_DIRECTION.md).

> **DS 인지형**: 골드 진실 = GameState(`Team1/2Gold`). 구매는 서버 권위(Server RPC). 클라는 `DT_Items` 카탈로그를 직접 로드해 목록만 그린다.

---

## A. 목적 & 진입/이탈

라운드 준비 중 **유닛 귀속 아이템**을 사는 팝업. 2뷰 흐름:
- **View1(유닛 선택)**: 배치된 유닛(최대 5)을 골라 그 유닛의 아이템 페이지로.
- **View2(아이템)**: 선택 유닛의 구매 가능 아이템 목록(★추천 상단 정렬), 뒤로/닫기.
- 헤더의 남은시간/골드는 두 뷰 모두에서 항상 표시.

| 시점 | 동작 | 가드 |
|------|------|------|
| `ShopButton` 클릭 | `OpenForUnits(배치유닛들)` → Visible, View1 | 로컬 위젯 상호작용 |
| 준비 단계 종료 | 자동 CloseShop | 준비/정산 단계에만 구매 허용 |
| `BackButton` | View2 → View1 | — |
| `CloseButton` | Collapsed(준비 화면 복귀) | — |

- 아이템은 **유닛(UnitId=로스터 인덱스) 귀속, 라운드 누적**. 한 유닛은 매 라운드 리스폰돼도 아이템 재적용.

---

## B. 레이아웃 존

전체화면 반투명 오버레이(`BgDeep` dim) 위 중앙 패널:

```
┌─ PanelBorder (Border, 토큰 솔리드) ───────────────────┐
│ TitleText(H1)          [타이머칩 Info]  [골드칩 Gold]  │
│ ── HeaderDivider(옵션) ─────────────────────────────── │
│                                                        │
│  View1: UnitPickerBox (WrapBox)                        │
│    [유닛][유닛][유닛][유닛][유닛]                       │
│                                                        │
│  View2: ItemStoreBox (VerticalBox)                     │
│    ★[추천 아이템 — Gold 가격]                          │
│    ★[추천 아이템]                                       │
│     [일반 아이템] …                                     │
│                                                        │
│ [BackButton(보조)]                    [CloseButton]    │
└────────────────────────────────────────────────────────┘
```

- View1/View2 는 같은 패널 안에서 전환(하나만 표시). 헤더는 공통.

---

## C. 상태 & 변형

| 상태 | 조건 | 표시 |
|------|------|------|
| **empty** | 배치된 유닛 0 | "배치된 유닛이 없습니다" 안내, 구매 불가 |
| **populated** | 배치 유닛 ≥1 | View1 유닛 버튼, 선택 시 View2 아이템 |

- 구매 가능/불가: 골드 충분=`Success`/Gold 활성, 부족=`TextFaint` 비활성(`SetAffordable`).
- 추천 아이템(★)은 상단 정렬 + Gold 강조.

---

## D. ⭐ 데이터 명세 (DS 인지)

| 데이터 요소 | 출처 시스템 | GameState 복제? | OnRep_ 필요? | 클라 접근 | 갱신 빈도 |
|-------------|------------|:---------------:|:------------:|-----------|-----------|
| 남은 준비 시간(`TimerText`) | 서버 준비 타이머 | 서버 권위 | — | `CharacterSelect` → `UpdateTimer(sec)` | 매 프레임 |
| 팀 골드(`GoldText`) | GameState `Team1/2Gold` | ✅ | ✅ 델리게이트 | `OnTeamGoldChanged(Team,Gold)` 구독 + `GetAOSGameStateChecked()` | 골드 변동 시 |
| 배치 유닛 목록(`UnitPickerBox`) | `CharacterSelect` 로컬 배치 상태 | — | — | `OpenForUnits(TArray<FAOSShopUnit>)` | 상점 열 때 1회 |
| 아이템 카탈로그(`ItemStoreBox`) | `DT_Items`(`/Game/AOS/GAS/Data/DT_Items`) | ❌(정적 데이터) | — | 클라가 `LoadItemTable()` 직접 로드 | 유닛 선택 시 |
| 유닛별 보유 아이템 수 | GameState/서버 인벤(귀속) | ✅ | — | 유닛 버튼 라벨(OwnedItemCount) | 구매 반영 시 |

- **규칙**: 골드/보유는 `GetGameState<AAOSGameState>()` 경유. `GetAuthGameMode()` 금지.
- `DT_Items` 는 정적 카탈로그라 클라가 직접 로드해도 안전(복제 대상 아님). **구매 판정만 서버.**
- 골드는 델리게이트(`OnTeamGoldChanged`) 구독으로 실시간 갱신 → 구매 직후 즉시 반영/구매가능성 재계산.

---

## E. 상호작용 맵 (클라 → 서버)

| 요소 | 액션 | 즉시 피드백 | 서버 RPC 진입점 |
|------|------|-------------|-----------------|
| 유닛 버튼(`UnitPickerBox`) | 클릭 | View2(해당 유닛 아이템)로 전환 | — (로컬 뷰 전환) |
| 아이템 버튼(`ItemStoreBox`) | 클릭 | (구매 성공 시 골드 차감 반영) | `HandleBuy(UnitId,RowName)` → PC `Server_BuyItemForUnit` |
| `BackButton` | 클릭 | View2 → View1 | — |
| `CloseButton` | 클릭 | 팝업 Collapsed | — |

- 클라는 구매 **요청**만. 골드 충분성/차감/유닛 귀속 적용(Infinite GE `BP_GE_Item_*`)은 **서버 권위**.
- 구매 반영은 GameState 골드 복제 → `OnTeamGoldChanged` → 목록 구매가능성 재계산으로 닫힘.

---

## F. 스타일 계약

- 색/폰트/메트릭 = **전부 `AOSUIStyle` 토큰**. 인라인 색 리터럴 금지.
- **`PanelBorder`(옛 `RootBg`)는 토큰 솔리드**(`PanelBase`/`PanelRaised`/`Border`) — 기존 파일럿 장식 프레임 텍스처(`T_UI_Shop_Panel`/`_Button`/`_Divider`/`_Tooltip`)는 **제거**됨. 크롬은 텍스처 아님.
- 액센트 역할: 타이머=`Info`, 골드/가격/구매=`Gold`, 뒤로/닫기=보조(`PanelRaised`), 추천=Gold 강조, 구매가능=`Success`.
- 가격/골드 수치=monospace tabular-nums. 텍스트는 텍스처에 굽지 않음(전부 TextBlock).
- 아이템 **아이콘(콘텐츠)** 만 AI 아트/`KitBrush` 허용, 없으면 솔리드 폴백 → 텍스처 부재에도 PIE 정상.

---

## G. BindWidget 계약 & 실현 순서

**정적(WBP 이관, `meta = (BindWidgetOptional)`):**

| 프로퍼티 | 타입 | 역할 | 액센트 |
|----------|------|------|--------|
| `PanelBorder` | `UBorder` | 루트 패널 배경(토큰 솔리드, 장식 텍스처 없음) | Border |
| `TitleText` | `UTextBlock` | 상점 제목 | — |
| `TimerText` | `UTextBlock` | 남은 준비 시간 | Info |
| `GoldText` | `UTextBlock` | 팀 골드 | Gold |
| `BackButton` | `UButton` | View2 → View1 | 보조 |
| `CloseButton` | `UButton` | 팝업 닫기 | 보조 |
| `UnitPickerBox` | `UWrapBox` | View1 유닛 선택 컨테이너 | — |
| `ItemStoreBox` | `UVerticalBox` | View2 아이템 컨테이너 | — |
| `HeaderDivider` | `UImage`/`UBorder` (옵션) | 헤더 구획선 | Border |

**동적(C++ 유지):**
- 유닛 버튼: `OpenForUnits()` → `UAOSShopUnitButton` 생성해 `UnitPickerBox` 에 채움.
- 아이템 버튼: `RebuildItemButtons()` → `UAOSShopItemButton` 생성해 `ItemStoreBox` 에 채움(★추천 상단 정렬).

**실현 순서**:
1. `CharacterSelect` 가 자식으로 `UAOSShopWidget` 생성 (`IsLocalPlayerController()` 하위) → `BuildShopUI()` 로 정적 구조 1회 구축, 평소 Collapsed.
2. `ShopButton` 클릭 → `OpenForUnits(배치유닛)` — `UnitPickerBox` 채운 뒤 Visible.
3. 유닛 선택 → `RebuildItemButtons()` 로 `ItemStoreBox` 채운 뒤 View2 표시.
4. 골드 델리게이트 구독은 열릴 때 1회(`bSubscribed`), `NativeDestruct`/닫기에서 해제.

---

## H. 수용 기준 (테스트 가능)

- [ ] **DS 2-Client 검증**: Play As Dedicated Server / Listen Server 2-Client 에서 상점 열기→유닛 탭→구매(골드 차감)→닫기 정상.
- [ ] **구매 서버 권위**: 클라 클릭 → `Server_BuyItemForUnit` → GameState 골드 복제 → 두 클라 골드/구매가능성 동기.
- [ ] **초기 복제 타이밍**: 상점 열 때 골드가 즉시 표시(CDO 기본값 함정), 구매 직후 실시간 갱신.
- [ ] **empty 상태**: 배치 유닛 0 에서 안내 표시 + 구매 차단 + 크래시 없음.
- [ ] **텍스처 제거 후 렌더**: `T_UI_Shop_*` 없이 토큰 솔리드로 패널/버튼/구획선 정상 렌더.
- [ ] **에디터 편집성**: `WBP_Shop` 을 에디터에서 열어 계층(헤더/두 뷰 컨테이너) 편집 가능.
- [ ] **스타일 규율**: 인라인 색 없음, 크롬 솔리드, 골드=Gold·타이머=Info 역할 준수.

---

## ⚠️ 현재 코드와의 Drift (구현 트랙 참고)

`UAOSShopWidget`(현재 헤더)는 `BuildShopUI()` 로 서브트리를 **전량 프로그래밍 생성**하며 멤버가 plain `UPROPERTY()`(BindWidget 아님). 목표 계약과 차이:
- `RootBg`(현재 `UBorder`) → **`PanelBorder`**(목표 이름).
- **미존재 → 신규(옵션)**: `HeaderDivider`.
- 나머지(`TitleText`/`TimerText`/`GoldText`/`BackButton`/`CloseButton`/`UnitPickerBox`/`ItemStoreBox`)는 이름 일치.
- 스타일: 현재 `.cpp` 는 파일럿 `T_UI_Shop_*` 텍스처를 `KitBrush`/`KitButtonStyle` 로 배선 → **토큰 솔리드(`SolidBrush`/`SolidButtonStyle`)로 교체** 대상.

C++ 마이그레이션 트랙이 `BindWidgetOptional` 하이브리드 + 솔리드 크롬으로 전환한다. 이 스펙은 그 목표 계약을 규정한다.
