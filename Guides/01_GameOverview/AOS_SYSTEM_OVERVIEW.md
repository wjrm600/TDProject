# AOS (Auto Oriented Strategy) 게임 시스템 개요

## 프로젝트 구조

프로젝트는 다음과 같은 핵심 클래스들로 구성되어 있습니다:

### 1. **AOSGameMode** (AOSGameMode.h/cpp)
- 게임의 메인 모드 클래스
- 게임 상태 관리 (준비, 진행, 종료)
- 라운드 시간 관리 (기본값: 10분/600초)
- 승리 조건 확인 (상대 커맨드 센터 파괴)
- 팀별 캐릭터 배치 관리

**주요 기능:**
```cpp
- StartGame(): 게임 시작
- EndGame(WinningTeam): 게임 종료
- DeployCharacters(Team, LaneAssignments): 캐릭터 배치
- CheckVictoryConditions(): 승리 조건 확인
```

### 2. **AOSCharacter** (AOSCharacter.h/cpp)
- 플레이어가 조종하는 기본 캐릭터 클래스
- 각 플레이어는 4마리 캐릭터 보유
- 라인 할당 시스템 (탑, 미드, 바텀)

**주요 속성:**
```cpp
- Team: 팀 정보 (Team1/Team2)
- AssignedLane: 할당된 라인
- MaxHealth: 최대 체력 (기본값: 100)
- AttackDamage: 공격 데미지 (기본값: 10)
- AttackRange: 공격 범위 (기본값: 500)
```

### 3. **AOSAIController** (AOSAIController.h/cpp)
- 캐릭터의 자동 행동 관리
- 라인 순찰 시스템
- 적 탐지 및 자동 공격
- AI 상태 관리 (Idle, Patrolling, Attacking, Retreating)

**AI 행동 흐름:**
```
1. 순찰: 할당된 라인을 따라 움직임
2. 탐지: 근처 적(거리 범위 내)을 감지
3. 공격: 적을 향해 이동하고 공격 범위 내에서 공격
4. 조건 충족 시 다시 순찰로 복귀
```

### 4. **AOSStructure** (AOSStructure.h/cpp)
- 맵에 배치되는 구조물 (타워, 커맨드 센터)
- 체력 관리 및 파괴 시스템
- 자동 방어 시스템 (범위 내 적을 자동 공격)

**구조물 종류:**
- **Tower**: 각 라인 3개씩 배치 (체력: 1000)
- **CommandCenter**: 팀 본진 중심 (체력: 5000, 파괴 시 게임 패배)

### 5. **AOSPlayerController** (AOSPlayerController.h/cpp)
- 플레이어 입력 및 게임 상태 관리
- 캐릭터 배치 인터페이스
- 준비 단계 → 게임 시작 관리

**주요 기능:**
```cpp
- SetCharacterDeployment(LaneAssignments): 4마리 캐릭터의 라인 배치 설정
- DeployCharactersToLanes(): 설정된 배치에 따라 캐릭터 배포
- StartGameFromPreparation(): 준비 단계에서 게임 시작
```

### 6. **AOSMapManager** (AOSMapManager.h/cpp)
- 맵 레이아웃 관리
- 3개 라인 설정 (Top, Mid, Bottom)
- 각 라인별 타워 및 커맨드 센터 위치 관리
- 동적 구조물 생성

**기본 맵 설정:**
```
Top Lane:    Team1(2000, 2000, 0) ↔ Team2(-2000, -2000, 0)
Mid Lane:    Team1(2000, 0, 0) ↔ Team2(-2000, 0, 0)
Bottom Lane: Team1(2000, -2000, 0) ↔ Team2(-2000, 2000, 0)
```

## 게임 플로우

### 단계 1: 준비 (Preparation)
- 각 플레이어가 자신의 4마리 캐릭터를 3개 라인에 배치
- 예시: [Top, Mid, Bottom, Mid] → 탑 1마리, 미드 2마리, 바텀 1마리

### 단계 2: 게임 진행 (GameRunning)
- 게임 타이머 시작 (10분)
- 모든 캐릭터가 AI에 의해 자동으로 라인을 순찰하고 전투
- 맵의 타워들도 범위 내 적을 자동 공격

### 단계 3: 게임 종료 (GameEnded)
- 상대 팀의 커맨드 센터 파괴: 즉시 승리
- 시간 만료: 점수로 승자 결정 (미구현)

## 네트워크 플레이

현재 기본 구조만 완성되었으며, 네트워크 플레이는 다음 단계에서 구현됩니다:

```cpp
// 필요한 작업:
1. Replication 설정 추가 (bReplicates = true는 이미 설정)
2. RPC (Remote Procedure Call) 함수 추가
3. Blueprintable 클래스 생성 및 배포
4. 네트워크 동기화 변수 추가
```

## 다음 단계

### 1. 블루프린트 클래스 생성
- AOSGameMode, AOSCharacter, AOSStructure 등의 블루프린트 버전 생성
- 비주얼 및 애니메이션 추가

### 2. UI 시스템
- 준비 단계 UI (캐릭터 배치 인터페이스)
- 게임 화면 UI (타이머, 체력 표시, 상황 정보)
- 게임 종료 화면

### 3. 네트워크 멀티플레이
- Replication 함수 구현
- 플레이어 간 동기화
- 세션 매칭 시스템

### 4. 게임 폴리시 및 밸런싱
- 캐릭터 속성 조정
- 타워/구조물 밸런싱
- 게임 진행 속도 조정

### 5. 사운드 및 이펙트
- 공격 사운드
- 타워 공격 이펙트
- 구조물 파괴 이펙트

## 파일 구조

```
Source/TDProject/AOS/
├── AOSGameMode.h/cpp          # 게임 모드
├── AOSCharacter.h/cpp         # 플레이어 캐릭터
├── AOSAIController.h/cpp       # AI 컨트롤러
├── AOSPlayerController.h/cpp   # 플레이어 컨트롤러
├── AOSStructure.h/cpp         # 타워/커맨드 센터
└── AOSMapManager.h/cpp        # 맵 관리
```

## 사용 예시

### 게임 모드 설정
1. 새 레벨 생성
2. World Settings에서 Game Mode Override를 "AOSGameMode_C" 로 설정
3. AOSMapManager 배치

### 캐릭터 배치
```cpp
// 플레이어 1
TArray<EAOSLane> Deployment = {EAOSLane::Top, EAOSLane::Mid, EAOSLane::Bottom, EAOSLane::Mid};
PlayerController->SetCharacterDeployment(Deployment);
PlayerController->DeployCharactersToLanes();

// 게임 시작
PlayerController->StartGameFromPreparation();
```

## 확장 가능성

이 시스템은 다음과 같이 확장 가능합니다:

1. **캐릭터 클래스**: 다양한 특성을 가진 서브클래스 생성 (탱커, 딜러, 서포터 등)
2. **AI 개선**: StateTree를 사용한 고급 AI 로직
3. **아이템 시스템**: 캐릭터가 습득할 수 있는 아이템
4. **스킬 시스템**: 각 캐릭터의 고유 스킬 추가
5. **포탑 시스템**: 플레이어가 배치 가능한 추가 방어 구조물

---

**마지막 업데이트**: 2025-11-16
**상태**: 기본 시스템 완성, 테스트 및 네트워크 구현 필요
