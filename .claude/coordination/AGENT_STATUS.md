# 에이전트 상태 추적

> ⚠️ **이 표는 수동 유지라 drift 한다.** 실제 상태의 **단일 진실은 git**:
> - 활성 worktree: `git worktree list`
> - 에이전트 브랜치: `git branch --list 'agent/*'`
> - 머지 여부: PR 또는 `git branch --merged main`
>
> 그래서 이 파일은 "지금 누가 뭘 한다"의 가벼운 메모일 뿐, 권위는 git 에 있다.
> (`/multi-agent`·`/merge-agents` 가 갱신하지만, 누락되면 위 git 명령으로 역추적할 것.)

## 미해결 (사용자 결정 필요)

| 브랜치 | 커밋 | 내용 | 결정 |
|--------|------|------|------|
| `agent/prog-ui/hp-bar-percent` | `ceae1a3` | HP 바 퍼센트 수치 텍스트 표시 | **머지 or 삭제?** (worktree 는 제거됨, 브랜치만 잔존) |

> 2026-06-20 정리 때 발견: 아래 과거 표에 "완료(2026-04-29)"로 적혀 있던 이 작업이
> 실제로는 main 에 **머지되지 않은** 채 worktree 만 4개월 방치돼 있었다 — 표↔현실 drift 의 실사례.
> 위생 정리에서 worktree(디스크 복제본)는 제거하고 브랜치(커밋 `ceae1a3`)는 보존했다.

## 현재 활성 세션

(없음 — `git worktree list` 로 확인. main 외 worktree 가 없으면 활성 프로그래머 세션 없음.)

## 운영 메모 (1사이클 = 머지 + 정리까지)

프로그래머 도메인 1사이클은 **worktree 생성 → 작업 → 머지 → worktree 제거 → 브랜치 정리**.
마지막 두 단계를 빼먹으면 위 prog-ui 처럼 방치 worktree·dangling 브랜치로 drift 한다.
머지 순서/방식은 `/merge-agents` 및 CLAUDE.md "Multi-Agent Workflow" 참고.
