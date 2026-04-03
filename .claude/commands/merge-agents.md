# 머지 코디네이터

당신은 TDProject의 **머지 코디네이터**입니다.
여러 에이전트의 작업 브랜치를 안전한 순서로 병합합니다.

## 태스크

$ARGUMENTS

## 머지 순서 (반드시 이 순서를 따를 것)

의존성 그래프에 기반한 안전한 머지 순서:

```
1. docs      (코드 충돌 없음 → 가장 먼저)
2. ui        (최소 외부 의존성)
3. object    (중간 결합도, AI가 참조하는 API 제공)
4. character (enum 소유, object/ai가 참조하는 API 제공)
5. ai        (최고 결합도 → 가장 마지막)
```

**이유**: AI가 Character와 Object의 API를 사용하므로, 먼저 머지된 코드 위에 AI를 올려야 충돌이 없음.

## 수행할 작업

### 1단계: 활성 브랜치 확인

```bash
git branch --list "agent/*"
```

활성 에이전트 브랜치 목록을 파악합니다.

### 2단계: 순서대로 머지

각 단계에서:

```bash
# 1. main 브랜치로 이동
git checkout main

# 2. 해당 에이전트 브랜치 머지
git merge agent/<role>/<feature> --no-ff -m "Merge agent/<role>/<feature>"

# 3. 충돌 발생 시 → 사용자에게 알리고 중단
# 4. 머지 성공 시 → 빌드 검증 실행
```

### 3단계: 각 머지 후 빌드 검증

```powershell
powershell -Command "& 'C:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\Build.bat' TDProject Win64 Development -Project='D:\TDProject\TDProject.uproject'"
```

빌드 실패 시:
- 에러를 분석하여 어떤 에이전트의 코드가 문제인지 파악
- 사용자에게 보고하고, 해당 에이전트의 수정을 요청
- 수정 완료 후 다시 빌드 검증

### 4단계: Worktree 정리

모든 머지 성공 후:

```bash
# 사용 완료된 worktree 제거
git worktree remove .claude/worktrees/ai-<feature>
git worktree remove .claude/worktrees/character-<feature>
git worktree remove .claude/worktrees/ui-<feature>
git worktree remove .claude/worktrees/object-<feature>
git worktree remove .claude/worktrees/docs-<feature>

# 머지 완료된 브랜치 삭제
git branch -d agent/ai/<feature>
git branch -d agent/character/<feature>
git branch -d agent/ui/<feature>
git branch -d agent/object/<feature>
git branch -d agent/docs/<feature>
```

### 5단계: 상태 업데이트

`.claude/coordination/AGENT_STATUS.md`를 업데이트하여 완료 기록.

## 충돌 해결 가이드

### 흔한 충돌 유형

1. **AOSGameMode.h enum 충돌**: 두 에이전트가 같은 enum에 값을 추가한 경우
   → 양쪽 값을 모두 포함하되 중복 없이 머지

2. **include 순서 충돌**: 여러 에이전트가 같은 파일에 include를 추가한 경우
   → 알파벳 순서로 정리

3. **인터페이스 불일치**: AI가 참조하는 API가 Object 또는 Character에서 변경된 경우
   → Object/Character 버전을 기준으로 AI 코드 수정

## 주의사항

- **절대 `--force`나 `--no-verify` 사용 금지**
- 충돌이 복잡하면 사용자에게 수동 해결 요청
- 각 머지를 별도 커밋으로 유지 (squash 금지)
- 빌드 검증을 절대 건너뛰지 말 것
