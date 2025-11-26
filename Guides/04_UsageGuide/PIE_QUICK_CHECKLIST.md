# 🚀 PIE 테스트 빠른 체크리스트

## 1️⃣ 레벨 설정 (5분)

- [ ] 새 레벨 생성 → `AOS_TestLevel` 저장
- [ ] **World Settings** 열기
  - [ ] Game Mode Override: `AOSGameMode` 설정
- [ ] 바닥/조명 추가

## 2️⃣ 스폰 포인트 배치 (10분)

### Team1 (Red) - 맵 위쪽
- [ ] **Top Lane**
  - [ ] `SP_Team1_Top_0` (1500, 1500, 100) → Team1, Index 0
  - [ ] `SP_Team1_Top_1` (1300, 1300, 100) → Team1, Index 1

- [ ] **Mid Lane**
  - [ ] `SP_Team1_Mid_0` (1500, 0, 100) → Team1, Index 0
  - [ ] `SP_Team1_Mid_1` (1300, 0, 100) → Team1, Index 1

- [ ] **Bottom Lane**
  - [ ] `SP_Team1_Bottom_0` (1500, -1500, 100) → Team1, Index 0
  - [ ] `SP_Team1_Bottom_1` (1300, -1300, 100) → Team1, Index 1

### Team2 (Blue) - 맵 아래쪽
- [ ] **Top Lane**
  - [ ] `SP_Team2_Top_0` (-1500, -1500, 100) → Team2, Index 0
  - [ ] `SP_Team2_Top_1` (-1300, -1300, 100) → Team2, Index 1

- [ ] **Mid Lane**
  - [ ] `SP_Team2_Mid_0` (-1500, 0, 100) → Team2, Index 0
  - [ ] `SP_Team2_Mid_1` (-1300, 0, 100) → Team2, Index 1

- [ ] **Bottom Lane**
  - [ ] `SP_Team2_Bottom_0` (-1500, 1500, 100) → Team2, Index 0
  - [ ] `SP_Team2_Bottom_1` (-1300, 1300, 100) → Team2, Index 1

## 3️⃣ 캐릭터 블루프린트 (5분)

- [ ] Content Browser → Create → Blueprint Class
- [ ] Parent Class: `AAOSCharacter` 선택
- [ ] 이름: `BP_AOSCharacter`
- [ ] 기본 메시/머티리얼 추가 (선택)
- [ ] Compile & Save

## 4️⃣ 캐릭터 배치 (5분)

### Team1
- [ ] **Char_Team1_1** (1400, 800, 100)
  - Team: Team1
  - AssignedLane: Top

- [ ] **Char_Team1_2** (1400, 0, 100)
  - Team: Team1
  - AssignedLane: Mid

### Team2
- [ ] **Char_Team2_1** (-1400, -800, 100)
  - Team: Team2
  - AssignedLane: Top

- [ ] **Char_Team2_2** (-1400, 0, 100)
  - Team: Team2
  - AssignedLane: Mid

## 5️⃣ Level Blueprint 설정 (2분)

- [ ] Blueprints → Open Level Blueprint
- [ ] Event BeginPlay 추가
- [ ] Get Game Mode 추가
- [ ] Cast to AOSGameMode 추가
- [ ] Call Start Game 추가
- [ ] 연결:
  ```
  Event BeginPlay → Get Game Mode
                  ↓
           Cast to AOSGameMode
                  ↓
            Call Start Game
  ```
- [ ] Compile & Save

## 6️⃣ PIE 테스트 실행 (1분)

- [ ] **Play** 버튼 클릭 (또는 Alt+P)
- [ ] 게임 시작 후 관찰:

### ✅ 예상 결과
```
게임 시작 1초 후:

  Team1 캐릭터들이:
    └─ 각각 할당된 스폰 포인트로 이동
    └─ 예) Char_Team1_1 → SP_Team1_Top_0 위치로 이동

  Team2 캐릭터들이:
    └─ 각각 할당된 스폰 포인트로 이동
    └─ 예) Char_Team2_1 → SP_Team2_Top_0 위치로 이동

그 다음:
  └─ 각 캐릭터가 할당된 라인을 따라 순찰 시작
  └─ 게임 타이머 시작 (600초)
```

## 🐛 문제가 발생하면

| 문제 | 체크사항 |
|------|---------|
| 캐릭터가 이동 안 함 | Game Mode 설정 확인, 스폰 포인트 Team 설정 확인 |
| 게임 안 시작됨 | Level Blueprint의 Start Game 호출 확인 |
| 캐릭터가 겹침 | 스폰 포인트 위치 간격 확인 (최소 200 units) |
| 에러 메시지 | 콘솔(`) 열어서 에러 확인 |

## 💡 팁

### 빠른 확인 방법
1. 게임 시작 후 **Escape** 키로 일시정지
2. 각 캐릭터의 현재 위치 확인
3. 스폰 포인트 위치와 비교

### 시각적 디버깅
- 스폰 포인트 위에 텍스트 렌더러 추가
- 각 스폰 포인트마다 다른 색상 사용

### 콘솔 명령어
```
이 명령어들을 게임 중 물음표(`) 누른 후 입력:

stat unit           # FPS 확인
showdebug ai        # AI 상태 확인
```

---

**예상 소요 시간**: 약 30분

**완료 후**: 스폰 포인트 시스템이 정상 작동하는 것을 확인할 수 있습니다!
