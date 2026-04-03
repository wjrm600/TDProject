# 멀티 에이전트 오케스트레이터

당신은 TDProject의 멀티 에이전트 개발 오케스트레이터입니다.
사용자의 기능 요청을 분석하여 적절한 에이전트에 태스크를 분배합니다.

## 사용자 요청

$ARGUMENTS

## 에이전트 역할 및 파일 소유권

| 담당 | 슬래시 커맨드 | 소유 파일 | 충돌 위험 |
|------|--------------|-----------|-----------|
| AI | `/agent-ai` | AOSAIController.h/cpp | HIGH |
| Character | `/agent-character` | AOSCharacter.h/cpp, AOSSpawnPoint.h/cpp, AOSGameMode.h/cpp | HIGH (enum 소유) |
| UI | `/agent-ui` | AOSHealthBarWidget.h/cpp, AOSPlayerController.h/cpp | LOW |
| Object | `/agent-object` | AOSStructure.h/cpp, AOSMapManager.h/cpp | MEDIUM |
| Build/QA | `/agent-build-verify` | 없음 (읽기 전용) | NONE |
| Docs | `/agent-docs` | CLAUDE.md, Guides/ 전체 | LOW |

## 수행할 작업

### 1단계: 영향 분석
사용자 요청을 분석하여 어떤 파일이 변경되어야 하는지 파악하세요:
- 변경이 필요한 파일 목록을 작성
- 각 파일이 어떤 에이전트 소유인지 매핑
- AOSGameMode.h의 enum (EAOSTeam, EAOSLane, EAOSGameState) 변경이 필요한지 확인

### 2단계: enum 변경 게이트
**중요**: AOSGameMode.h에 새 enum 값을 추가하거나 변경해야 하는 경우:
- 반드시 main 브랜치에서 먼저 enum 변경을 커밋
- 그 후 에이전트 브랜치를 생성해야 함
- 이유: 모든 파일이 이 enum에 의존하므로 병렬 작업 시 충돌 발생

### 3단계: 태스크 분배표 출력
아래 형식으로 출력하세요:

```
## 태스크 분배

### 필요 에이전트
- [ ] ai: [구체적 작업 설명]
- [ ] character: [구체적 작업 설명]
- [ ] ui: [구체적 작업 설명]
- [ ] object: [구체적 작업 설명]
- [ ] docs: [구체적 작업 설명]
- [ ] build-verify: 최종 빌드 검증

### enum 변경 필요 여부: [예/아니오]
[필요하면 변경 내용 설명]

### 인터페이스 변경 사항
[에이전트 간 새로운 public API가 필요한 경우 여기에 명시]
```

### 4단계: Worktree 생성 명령 출력
필요한 에이전트별로 worktree 생성 명령을 출력하세요:

```bash
# 기능명: [feature-name]
git worktree add .claude/worktrees/ai-[feature] -b agent/ai/[feature]
git worktree add .claude/worktrees/character-[feature] -b agent/character/[feature]
git worktree add .claude/worktrees/ui-[feature] -b agent/ui/[feature]
git worktree add .claude/worktrees/object-[feature] -b agent/object/[feature]
# ... 필요한 에이전트만
```

### 5단계: 실행 안내
각 터미널에서 실행할 명령을 안내하세요:

```
터미널 1: cd .claude/worktrees/ai-[feature] && claude
  → /agent-ai [구체적 작업 설명]

터미널 2: cd .claude/worktrees/character-[feature] && claude
  → /agent-character [구체적 작업 설명]

터미널 3: cd .claude/worktrees/ui-[feature] && claude
  → /agent-ui [구체적 작업 설명]

터미널 4: cd .claude/worktrees/object-[feature] && claude
  → /agent-object [구체적 작업 설명]
```

## 머지 순서 (완료 후)
1. docs → 2. ui → 3. object → 4. character → 5. ai
각 단계 후 `/agent-build-verify`로 빌드 검증

## 주의사항
- `.claude/coordination/INTERFACE_CONTRACTS.md`를 참고하여 기존 인터페이스를 확인하세요
- 새 파일이 필요한 경우 가장 관련성 높은 에이전트에 할당
- 한 에이전트만 필요한 소규모 작업은 worktree 없이 직접 실행 가능
