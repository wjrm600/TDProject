# 📚 AOS 프로젝트 문서 색인

**위치**: `Guides/` 폴더 (카테고리별로 정리됨)

프로젝트 진행 과정에서 생성된 모든 문서의 색인입니다.

---

## 📁 폴더 구조

```
Guides/
├── 01_GameOverview/        (게임 개요)
├── 02_ProgressLog/         (작업 진행 사항)
├── 03_ClassReview/         (클래스 리뷰)
└── 04_UsageGuide/          (사용 가이드)
```

---

## 🎮 01_GameOverview (게임 개요)

게임의 전체 구조와 시각적 이해를 돕는 문서들입니다.

### [AOS_SYSTEM_OVERVIEW.md](./01_GameOverview/AOS_SYSTEM_OVERVIEW.md)
- **내용**: 전체 게임 시스템 아키텍처
- **범위**: 고수준 설계
- **추천 대상**: 시스템 설계 이해 필요
- **주요 내용**:
  - 전체 시스템 구조
  - 클래스 설명
  - 게임 플로우
  - 상태 다이어그램

### [SPAWN_SYSTEM_DIAGRAM.md](./01_GameOverview/SPAWN_SYSTEM_DIAGRAM.md)
- **내용**: 시각적 다이어그램 및 예시
- **범위**: 맵 배치, 플로우차트
- **추천 대상**: 시각적 학습 선호
- **주요 내용**:
  - 맵 배치 다이어그램
  - 스폰 프로세스 플로우
  - 배치 예시

---

## 📋 02_ProgressLog (작업 진행 사항)

프로젝트의 진행 과정과 변경 계획을 정리한 문서들입니다.

### [PROJECT_PROGRESS_LOG.md](./02_ProgressLog/PROJECT_PROGRESS_LOG.md) ⭐ 메인 문서
- **내용**: 초기 요구사항부터 현재까지의 모든 진행 과정
- **분량**: 긴 문서 (매우 상세함)
- **추천 대상**: 프로젝트 전체 흐름 이해 필요
- **섹션**:
  - 초기 요구사항
  - Phase 1-5 진행 상황
  - 학습사항 및 기술 결정
  - 통계 및 다음 단계

### [AUTO_SPAWN_CHANGES.md](./02_ProgressLog/AUTO_SPAWN_CHANGES.md)
- **내용**: Approach A 변경사항 계획서
- **범위**: 초기 계획 문서
- **추천 대상**: 설계 의도 이해 필요
- **주요 내용**:
  - 변경 요약
  - 기존 방식 vs 새로운 방식
  - 수정할 파일
  - 상세 변경사항
  - 게임 실행 플로우

---

## 🔧 03_ClassReview (클래스 리뷰)

각 Phase별 기술 구현과 클래스 상세 설명입니다.

### [AUTO_SPAWN_IMPLEMENTATION.md](./03_ClassReview/AUTO_SPAWN_IMPLEMENTATION.md)
- **내용**: 자동 캐릭터 생성 시스템 구현
- **범위**: Phase 3 상세 설명
- **추천 대상**: 스폰 시스템 이해 필요
- **주요 내용**:
  - 구현 요약
  - 변경된 파일 목록
  - 코드 스니펫
  - 게임 플로우
  - PIE 테스트 방법

### [POSITION_FIX.md](./03_ClassReview/POSITION_FIX.md)
- **내용**: 캐릭터 위치 설정 문제 해결
- **범위**: Phase 4 상세 설명
- **추천 대상**: 위치 설정 문제 이해 필요
- **주요 내용**:
  - 문제 분석
  - 해결 방법 3가지
  - 변경 전후 비교
  - 다음 개선 사항

### [TOWER_SYSTEM_GUIDE.md](./03_ClassReview/TOWER_SYSTEM_GUIDE.md)
- **내용**: 타워 및 커맨드 센터 시스템 구현
- **범위**: Phase 5 상세 설명
- **추천 대상**: 타워 시스템 이해 필요
- **주요 내용**:
  - 시스템 개요
  - 타워 생성 프로세스
  - 클래스별 설명
  - 사용 방법 및 테스트

### [TOWER_CRASH_FIX.md](./03_ClassReview/TOWER_CRASH_FIX.md)
- **내용**: 타워 크래시 수정 및 기술 노트
- **범위**: LoadObject vs ConstructorHelpers
- **추천 대상**: 타워 로딩 에러 발생 시
- **주요 내용**:
  - 문제 분석
  - 해결책 (LoadObject 사용)
  - 기술 비교표
  - 다음 개선사항

### [CODE_CHANGES_REFERENCE.md](./03_ClassReview/CODE_CHANGES_REFERENCE.md)
- **내용**: 코드 변경사항 빠른 참고서
- **범위**: 파일별 정확한 라인 번호
- **추천 대상**: 특정 변경 위치 찾을 때
- **특징**: 변경 전/후 코드 비교, 테이블 형식

---

## 📖 04_UsageGuide (사용 가이드)

게임 실행, 테스트, 설정 방법을 정리한 실용 가이드들입니다.

### [QUICK_TOWER_TEST.md](./04_UsageGuide/QUICK_TOWER_TEST.md) ⭐ 즉시 테스트
- **내용**: 타워 빠른 테스트 (3분)
- **범위**: 빠른 설정 및 실행
- **추천 대상**: 지금 바로 타워를 보고 싶을 때
- **특징**: 단계별 명확한 지시사항

### [README_PIE_START.md](./04_UsageGuide/README_PIE_START.md)
- **내용**: PIE 테스트 시작 가이드 (종합)
- **범위**: 전체 설정 방법
- **추천 대상**: 게임 실행 준비
- **주요 내용**:
  - 30분 안에 하기
  - 빠른 시작 3가지 방법
  - 설정 체크리스트
  - 일반적인 문제 해결

### [PIE_QUICK_CHECKLIST.md](./04_UsageGuide/PIE_QUICK_CHECKLIST.md) ⭐ 빠른 시작
- **내용**: 30분 내 PIE 테스트 체크리스트
- **범위**: 최소 단계 설명
- **추천 대상**: 빨리 테스트하고 싶을 때
- **특징**: 체크박스 형식, 간단함

### [PIE_TEST_GUIDE.md](./04_UsageGuide/PIE_TEST_GUIDE.md)
- **내용**: PIE 테스트 단계별 상세 설명
- **범위**: 모든 설정 파라미터 포함
- **추천 대상**: 자세한 설명 필요
- **주요 내용**:
  - 단계별 진행
  - 스크린샷 예시 (추가 예정)
  - 문제 해결 섹션

### [UPDATED_SPAWN_SETUP.md](./04_UsageGuide/UPDATED_SPAWN_SETUP.md)
- **내용**: 스폰 포인트 상세 설정 가이드
- **범위**: Lane 속성 설정 방법
- **추천 대상**: 스폰 포인트 배치 시
- **주요 내용**:
  - 12개 스폰 포인트 설정 방법
  - Team/Lane/Index 설정
  - 체크리스트
  - 콘솔 로그 확인

### [SPAWN_POINT_SETUP.md](./04_UsageGuide/SPAWN_POINT_SETUP.md)
- **내용**: 스폰 포인트 기본 설정
- **범위**: 초기 설정 방법
- **추천 대상**: 스폰 시스템 기초 이해

### [TOWER_TEST_SETUP.md](./04_UsageGuide/TOWER_TEST_SETUP.md)
- **내용**: 타워 테스트 상세 설정 가이드
- **범위**: 전체 설정 방법 및 문제 해결
- **추천 대상**: 자세한 설명 필요
- **주요 내용**:
  - 에디터 설정
  - MapManager 배치
  - PIE 테스트 방법
  - 문제 해결

### [TOWER_SPAWNING_DEBUG.md](./04_UsageGuide/TOWER_SPAWNING_DEBUG.md)
- **내용**: 타워 스포닝 디버깅 가이드
- **범위**: 진단 및 해결책
- **추천 대상**: 타워가 안 보일 때
- **주요 내용**:
  - 증상 분석
  - 원인 분석
  - 디버그 로깅
  - 해결 방법

---

## 📚 빠른 접근 가이드

### 상황별 추천 문서

#### 🚀 "지금 바로 타워를 보고 싶다" (추천)
1. [04_UsageGuide/QUICK_TOWER_TEST.md](./04_UsageGuide/QUICK_TOWER_TEST.md) (3분)

#### 🎮 "지금 바로 게임을 실행하고 싶다"
1. [04_UsageGuide/PIE_QUICK_CHECKLIST.md](./04_UsageGuide/PIE_QUICK_CHECKLIST.md) (5분)
2. [04_UsageGuide/UPDATED_SPAWN_SETUP.md](./04_UsageGuide/UPDATED_SPAWN_SETUP.md) (15분)

#### 📖 "프로젝트 전체를 이해하고 싶다"
1. [02_ProgressLog/PROJECT_PROGRESS_LOG.md](./02_ProgressLog/PROJECT_PROGRESS_LOG.md) (메인 문서)
2. [01_GameOverview/AOS_SYSTEM_OVERVIEW.md](./01_GameOverview/AOS_SYSTEM_OVERVIEW.md) (아키텍처)

#### 🔧 "특정 부분을 깊이 있게 이해하고 싶다"
- 자동 생성: [03_ClassReview/AUTO_SPAWN_IMPLEMENTATION.md](./03_ClassReview/AUTO_SPAWN_IMPLEMENTATION.md)
- 위치 설정: [03_ClassReview/POSITION_FIX.md](./03_ClassReview/POSITION_FIX.md)
- 타워 시스템: [03_ClassReview/TOWER_SYSTEM_GUIDE.md](./03_ClassReview/TOWER_SYSTEM_GUIDE.md)
- 변경사항: [03_ClassReview/CODE_CHANGES_REFERENCE.md](./03_ClassReview/CODE_CHANGES_REFERENCE.md)

#### 🎯 "문제를 해결하고 싶다"
1. [04_UsageGuide/README_PIE_START.md](./04_UsageGuide/README_PIE_START.md) (일반적인 문제)
2. [04_UsageGuide/PIE_TEST_GUIDE.md](./04_UsageGuide/PIE_TEST_GUIDE.md) (자세한 설명)

---

## 🎯 추천 읽기 순서

### 처음 시작하는 경우

**1단계 (개요 파악)** - 5분
- [02_ProgressLog/PROJECT_PROGRESS_LOG.md](./02_ProgressLog/PROJECT_PROGRESS_LOG.md) 목차 읽기

**2단계 (전체 이해)** - 30분
- [01_GameOverview/AOS_SYSTEM_OVERVIEW.md](./01_GameOverview/AOS_SYSTEM_OVERVIEW.md) 읽기
- [02_ProgressLog/PROJECT_PROGRESS_LOG.md](./02_ProgressLog/PROJECT_PROGRESS_LOG.md) "1. 초기 요구사항" ~ "Phase 1" 읽기

**3단계 (실습 준비)** - 20분
- [04_UsageGuide/README_PIE_START.md](./04_UsageGuide/README_PIE_START.md) 읽기
- [04_UsageGuide/PIE_QUICK_CHECKLIST.md](./04_UsageGuide/PIE_QUICK_CHECKLIST.md) 스캔

**4단계 (실행)** - 30분
- [04_UsageGuide/PIE_QUICK_CHECKLIST.md](./04_UsageGuide/PIE_QUICK_CHECKLIST.md) 따라하기

### 프로젝트 개발에 참여하는 경우

**1단계 (전체 이해)** - 30분
- [02_ProgressLog/PROJECT_PROGRESS_LOG.md](./02_ProgressLog/PROJECT_PROGRESS_LOG.md) 전체 읽기

**2단계 (상세 이해)** - 60분
- 관심 있는 Phase별로 해당 문서 읽기
  - Phase 1: [01_GameOverview/AOS_SYSTEM_OVERVIEW.md](./01_GameOverview/AOS_SYSTEM_OVERVIEW.md)
  - Phase 2: [04_UsageGuide/UPDATED_SPAWN_SETUP.md](./04_UsageGuide/UPDATED_SPAWN_SETUP.md)
  - Phase 3: [03_ClassReview/AUTO_SPAWN_IMPLEMENTATION.md](./03_ClassReview/AUTO_SPAWN_IMPLEMENTATION.md)
  - Phase 4: [03_ClassReview/POSITION_FIX.md](./03_ClassReview/POSITION_FIX.md)
  - Phase 5: [03_ClassReview/TOWER_SYSTEM_GUIDE.md](./03_ClassReview/TOWER_SYSTEM_GUIDE.md)

**3단계 (코드 분석)** - 필요시
- [03_ClassReview/CODE_CHANGES_REFERENCE.md](./03_ClassReview/CODE_CHANGES_REFERENCE.md)로 정확한 위치 파악

---

## 📊 문서 통계

| 카테고리 | 문서 수 | 주요 목적 |
|---------|--------|---------|
| 01_GameOverview | 2개 | 게임 전체 구조 이해 |
| 02_ProgressLog | 2개 | 작업 진행 과정 추적 |
| 03_ClassReview | 5개 | 기술 구현 상세 검토 |
| 04_UsageGuide | 8개 | 실제 사용 방법 가이드 |
| **총계** | **17개** | **완전한 문서화** |

---

## ✨ 핵심 문서 3개

만약 시간이 부족하다면 이 3개만 읽으세요:

1. **[02_ProgressLog/PROJECT_PROGRESS_LOG.md](./02_ProgressLog/PROJECT_PROGRESS_LOG.md)**
   - 프로젝트 전체 흐름 이해

2. **[04_UsageGuide/QUICK_TOWER_TEST.md](./04_UsageGuide/QUICK_TOWER_TEST.md)**
   - 게임 즉시 실행

3. **[03_ClassReview/CODE_CHANGES_REFERENCE.md](./03_ClassReview/CODE_CHANGES_REFERENCE.md)**
   - 변경사항 정확히 파악

---

**최종 업데이트**: 2025-11-23
**카테고리화 완료**: ✅
**총 문서 수**: 17개
**총 분량**: ~6,500줄
