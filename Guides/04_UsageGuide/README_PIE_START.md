# 🎮 AOS 게임 - PIE 테스트 시작 가이드

> **목표**: Unreal Engine PIE(Play In Editor)에서 스폰 포인트에 캐릭터가 자동으로 배치되는 것을 확인합니다.

## 📚 문서 가이드

이 프로젝트의 모든 설정 정보는 다음 문서들에 정리되어 있습니다:

### 🚀 빠른 시작 (지금 봐야 할 것!)
- **[PIE_QUICK_CHECKLIST.md](PIE_QUICK_CHECKLIST.md)** ⭐
  - 30분 안에 PIE 테스트할 수 있는 체크리스트
  - 간단한 YES/NO 형식
  - **이것부터 시작하세요!**

### 📖 상세 설명서
- **[PIE_TEST_GUIDE.md](PIE_TEST_GUIDE.md)**
  - 단계별 상세 설명
  - 모든 설정 파라미터 포함
  - 스크린샷과 함께 진행 (추가 예정)
  - 문제 해결 섹션 포함

### 🗺️ 시각적 다이어그램
- **[SPAWN_SYSTEM_DIAGRAM.md](SPAWN_SYSTEM_DIAGRAM.md)**
  - 맵 배치 다이어그램
  - 스폰 프로세스 플로우차트
  - 배치 예시 (Top 2/Mid 1/Bottom 1, 등)

### 🛠️ 시스템 전체 설명
- **[AOS_SYSTEM_OVERVIEW.md](AOS_SYSTEM_OVERVIEW.md)**
  - 전체 시스템 아키텍처
  - 클래스 설명
  - 게임 플로우

## 🎯 30분 안에 하기

### 1️⃣ **레벨 준비** (5분)
```
✅ 새 레벨 생성
✅ Game Mode Override를 AOSGameMode로 설정
✅ 바닥 추가
```

### 2️⃣ **스폰 포인트 배치** (10분)
```
총 12개 스폰 포인트:
  ✅ Team1 Top/Mid/Bottom 각 2개 = 6개
  ✅ Team2 Top/Mid/Bottom 각 2개 = 6개
```

### 3️⃣ **캐릭터 준비** (8분)
```
✅ BP_AOSCharacter 블루프린트 생성
✅ 레벨에 4개 캐릭터 배치 (Team1 2개, Team2 2개)
✅ 각 캐릭터 Team/Lane 설정
```

### 4️⃣ **게임 시작 연결** (2분)
```
✅ Level Blueprint에서 Event BeginPlay → Start Game 연결
```

### 5️⃣ **PIE 실행** (5분)
```
✅ Play 버튼 클릭
✅ 캐릭터들이 스폰 포인트로 이동하는 것 확인
✅ AI가 라인 순찰을 시작하는 것 확인
```

## 🗂️ 코드 구조

### 핵심 C++ 클래스
```
Source/TDProject/AOS/
├── AOSGameMode.h/cpp              ← 게임 전체 관리 + 스폰 포인트 등록
├── AOSSpawnPoint.h/cpp            ← 캐릭터 스폰 위치
├── AOSCharacter.h/cpp             ← 플레이어 캐릭터
├── AOSAIController.h/cpp           ← 자동 라인 순찰
├── AOSStructure.h/cpp             ← 타워/커맨드 센터
├── AOSMapManager.h/cpp            ← 맵 레이아웃 (탑/미드/바텀)
└── AOSPlayerController.h/cpp       ← 플레이어 제어
```

### 주요 시스템 플로우
```
BeginPlay
  ├─ AOSGameMode::BeginPlay()
  │  └─ 모든 AAOSSpawnPoint 자동 등록
  │
  ├─ Level Blueprint Event BeginPlay
  │  └─ AOSGameMode::StartGame() 호출
  │
  └─ 각 캐릭터
     ├─ AAOSCharacter::DeployToLane()
     ├─ AOSGameMode::SpawnCharacter()
     └─ AAOSSpawnPoint::SetOccupiedCharacter()
```

## 💡 핵심 개념

### 스폰 포인트란?
- **위치**: 게임 시작 시 캐릭터가 나타날 위치
- **팀**: Team1(Red) 또는 Team2(Blue)
- **인덱스**: 같은 팀 내 순서 (0, 1, 2, 3...)

### 라인이란?
- **Top**: 맵 위쪽 라인
- **Mid**: 맵 중간 라인
- **Bottom**: 맵 아래쪽 라인

### 배치란?
- 플레이어가 4마리 캐릭터를 어느 라인에 배치할지 결정
- 예: `[Top, Mid, Bottom, Mid]` = 탑 1마리, 미드 2마리, 바텀 1마리

## 🚀 빠른 시작 3가지 방법

### 방법 1: 최소한의 설정 (15분) ⭐ 추천
```
1. 새 레벨 + Game Mode 설정
2. Team1/Team2 Mid Lane 스폰 포인트 4개만 배치
3. 캐릭터 2개 배치 (각 팀 1마리)
4. PIE 실행
```

### 방법 2: 완전한 설정 (30분)
```
1. 새 레벨 + Game Mode 설정
2. 모든 라인에 스폰 포인트 12개 배치
3. 캐릭터 4개 배치 (각 팀 2마리)
4. PIE 실행
```

### 방법 3: 코드로 자동화 (고급)
```
C++ 코드로 런타임 중에 스폰 포인트/캐릭터 자동 생성
(현재는 수동 배치로 시작)
```

## ⚙️ 설정 체크리스트

| 항목 | 설정값 | 확인 |
|------|--------|------|
| Game Mode | AOSGameMode | ☐ |
| 스폰 포인트 개수 | 12개 (선택: 4개 최소) | ☐ |
| 캐릭터 개수 | 4개 | ☐ |
| BP_AOSCharacter 생성 | 완료 | ☐ |
| Level Blueprint | Event BeginPlay → StartGame | ☐ |

## 🐛 일반적인 문제

### Q1: 캐릭터가 스폰 포인트로 이동하지 않아요
**A**:
- Game Mode 설정이 AOSGameMode인지 확인
- 스폰 포인트의 Team 설정이 캐릭터 Team과 일치하는지 확인
- Level Blueprint에서 StartGame()이 호출되는지 확인

### Q2: 캐릭터가 스폰되지 않아요
**A**:
- Content Browser에서 BP_AOSCharacter가 생성되었는지 확인
- 레벨에 BP_AOSCharacter 인스턴스가 배치되었는지 확인

### Q3: 게임이 시작되지 않아요
**A**:
- Level Blueprint를 열어서 Event BeginPlay가 있는지 확인
- StartGame() 함수가 연결되어 있는지 확인
- Compile 에러가 있는지 확인

### Q4: 캐릭터들이 겹쳐요
**A**:
- 스폰 포인트 간 거리를 200 units 이상으로 증가
- 각 라인에 충분한 스폰 포인트가 있는지 확인

## 📞 도움이 필요할 때

### 상세 가이드 참고
1. **빠른 체크리스트**: [PIE_QUICK_CHECKLIST.md](PIE_QUICK_CHECKLIST.md)
2. **상세 설명**: [PIE_TEST_GUIDE.md](PIE_TEST_GUIDE.md)
3. **다이어그램**: [SPAWN_SYSTEM_DIAGRAM.md](SPAWN_SYSTEM_DIAGRAM.md)

### 콘솔 명령어 (게임 중)
```
물음표(`) 키를 눌러서 콘솔을 열고:

stat unit              # FPS와 성능 확인
showdebug ai           # AI 상태 확인
showdebug character    # 캐릭터 정보 확인
```

## 🎉 다음 단계

성공적으로 PIE에서 스폰이 작동하면:

1. **타워 추가**
   - AAOSStructure를 레벨에 배치
   - 각 라인에 3개씩 배치

2. **커맨드 센터 추가**
   - 각 팀의 본진에 배치
   - EStructureType::CommandCenter로 설정

3. **카메라 설정**
   - 플레이어 시점 설정
   - 맵 전체가 보이는 카메라

4. **UI 추가**
   - 게임 타이머
   - 팀 정보
   - 캐릭터 체력

5. **네트워크 구현**
   - Replication 설정
   - 멀티플레이 동기화

## 📊 프로젝트 구조

```
TDProject/
├── Source/TDProject/
│   └── AOS/                    ← AOS 게임 시스템
│       ├── AOSGameMode.*       ← 게임 관리
│       ├── AOSSpawnPoint.*     ← 스폰 포인트 ⭐
│       ├── AOSCharacter.*      ← 캐릭터
│       ├── AOSAIController.*   ← AI
│       ├── AOSStructure.*      ← 건물
│       └── ...
├── PIE_QUICK_CHECKLIST.md      ← 빠른 시작 ⭐
├── PIE_TEST_GUIDE.md           ← 상세 가이드
├── SPAWN_SYSTEM_DIAGRAM.md     ← 다이어그램
├── AOS_SYSTEM_OVERVIEW.md      ← 전체 설명
└── ...
```

---

## 🚀 지금 바로 시작하기

1. **[PIE_QUICK_CHECKLIST.md](PIE_QUICK_CHECKLIST.md)** 열기
2. 체크리스트 따라가기
3. PIE 실행하기
4. 스폰 포인트에 캐릭터가 나타나는 것 확인!

**예상 소요 시간**: 30분 (최소: 15분)

**완료 후**: 기본 게임 루프가 작동하는 것을 확인할 수 있습니다! 🎮

---

**마지막 업데이트**: 2025-11-16
**상태**: 모든 C++ 시스템 완료, PIE 테스트 준비 완료
