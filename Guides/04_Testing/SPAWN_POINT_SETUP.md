# 스폰 포인트 설정 가이드

**통합 문서**: SPAWN_POINT_SETUP + UPDATED_SPAWN_SETUP (최신 내용 기준)
**최종 업데이트**: 2026-02-17

---

## 스폰 포인트 설정 항목

각 `AOSSpawnPoint` 액터의 Details 패널에서 4가지를 설정합니다:

```
┌─────────────────────────────────┐
│ AOS|Spawn                       │
├─────────────────────────────────┤
│ Team           ▼ (Team1/Team2) │
│ Lane           ▼ (Top/Mid/Bot) │
│ Spawn Index    0 (정수)        │
│ Spawn Enabled  ☑ (체크박스)    │
└─────────────────────────────────┘
```

- **Team**: 해당 스폰 포인트가 속한 팀
- **Lane**: 속한 레인 (Top / Mid / Bottom)
- **Spawn Index**: 같은 팀/레인 내 순서 (0부터 시작)
- **bSpawnEnabled**: false로 설정하면 이 포인트에서 캐릭터 스폰 안 함 (디버깅용)

---

## 12개 전체 설정표

### Team1 (Red)

| Actor 이름 | 위치 | Team | Lane | Index |
|----------|------|------|------|-------|
| SP_Team1_Top_0 | (1500, 1500, 100) | Team1 | Top | 0 |
| SP_Team1_Top_1 | (1300, 1300, 100) | Team1 | Top | 1 |
| SP_Team1_Mid_0 | (1500, 0, 100) | Team1 | Mid | 0 |
| SP_Team1_Mid_1 | (1300, 0, 100) | Team1 | Mid | 1 |
| SP_Team1_Bottom_0 | (1500, -1500, 100) | Team1 | Bottom | 0 |
| SP_Team1_Bottom_1 | (1300, -1300, 100) | Team1 | Bottom | 1 |

### Team2 (Blue)

| Actor 이름 | 위치 | Team | Lane | Index |
|----------|------|------|------|-------|
| SP_Team2_Top_0 | (-1500, -1500, 100) | Team2 | Top | 0 |
| SP_Team2_Top_1 | (-1300, -1300, 100) | Team2 | Top | 1 |
| SP_Team2_Mid_0 | (-1500, 0, 100) | Team2 | Mid | 0 |
| SP_Team2_Mid_1 | (-1300, 0, 100) | Team2 | Mid | 1 |
| SP_Team2_Bottom_0 | (-1500, 1500, 100) | Team2 | Bottom | 0 |
| SP_Team2_Bottom_1 | (-1300, 1300, 100) | Team2 | Bottom | 1 |

---

## Lane 설정의 중요성

Lane이 설정되어야 `GetNearestSpawnPoint(Team, Lane)` 함수가 올바르게 작동합니다:

```
GetNearestSpawnPoint(Team1, Top)
  → SP_Team1_Top_0 반환 (첫 번째 호출)
  → SP_Team1_Top_1 반환 (두 번째 호출)
  → null 반환 (초과)
```

Lane이 없으면 같은 팀의 아무 스폰 포인트에나 배치될 수 있습니다.

---

## bSpawnEnabled 디버깅 활용

한 번에 모든 캐릭터가 스폰되면 로그 추적이 어렵습니다.
특정 스폰 포인트 1-2개만 `bSpawnEnabled = true`로 두고 나머지는 `false`로 설정하면 로그 확인이 쉬워집니다.

---

## 배치 권장사항

1. **간격**: 같은 레인 내 스폰 포인트 간격 최소 200 units 이상 (캐릭터 겹침 방지)
2. **위치**: 레인 시작점(커맨드센터 쪽) 근처에 배치
3. **대칭**: Team1/Team2 포인트가 맵 양쪽에 대칭적으로 배치

---

## 콘솔 에러 확인

```
LogTemp Error: No available spawn point for Team X, Lane Y
```
→ 해당 팀/레인의 스폰 포인트 설정을 확인하거나 추가하세요.
