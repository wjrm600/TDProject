#!/usr/bin/env python3
"""
SessionStart 훅 — 세션 시작 시 프로젝트의 동적 상태(특히 멀티에이전트 drift)를 요약해 주입.

목적: cold-start 세션이 즉시 "지금 상황"을 인지하고, 조용히 썩는 것들
(머지 안 된 agent/* 브랜치, 방치 worktree, 미푸시 커밋)을 첫 화면에 노출.
정적 CLAUDE.md/메모리가 담을 수 없는 point-in-time 상태가 핵심 — AGENT_STATUS.md 수동 표 drift 보완.

비차단: 항상 exit 0. git 이 없거나 명령 실패해도 조용히 통과(세션을 막지 않음).
출력: stdout (Claude Code 가 SessionStart 컨텍스트로 주입).

수동 테스트: python .claude/hooks/session_start.py
"""
import subprocess
import sys


def git(*args):
    """git 명령 실행 → stdout(성공 시) 또는 "" (실패/예외). 절대 throw 안 함."""
    try:
        r = subprocess.run(["git", *args], capture_output=True, text=True, timeout=8)
        return r.stdout.strip() if r.returncode == 0 else ""
    except Exception:
        return ""


def _names(raw):
    """`git branch` 출력에서 '* '/공백 제거한 브랜치명 리스트."""
    return [ln.replace("*", "").strip() for ln in raw.splitlines() if ln.strip()]


def main():
    if git("rev-parse", "--is-inside-work-tree") != "true":
        sys.exit(0)  # git 저장소 아니면 조용히 통과

    branch = git("rev-parse", "--abbrev-ref", "HEAD") or "(detached)"
    head = git("rev-parse", "--short", "HEAD")
    dirty = len(_names(git("status", "--porcelain")))
    ahead = git("rev-list", "--count", "@{u}..HEAD")  # upstream 없으면 ""

    # ── drift 신호: agent/* 브랜치(머지 여부) + worktree ──
    agent_all = _names(git("branch", "--list", "agent/*"))
    agent_merged = set(_names(git("branch", "--merged", "main", "--list", "agent/*")))
    unmerged = [b for b in agent_all if b not in agent_merged]
    merged_leftover = [b for b in agent_all if b in agent_merged]
    extra_worktrees = [ln for ln in git("worktree", "list").splitlines()
                       if "/worktrees/" in ln.replace("\\", "/")]

    # ── 출력 조립 ──
    head_line = f"- 브랜치 `{branch}` @ `{head}`"
    head_line += f" · 미커밋 {dirty}개" if dirty else " · 작업트리 clean"
    if ahead and ahead != "0":
        head_line += f" · origin 대비 +{ahead} 미푸시"

    out = ["[세션 시작 상태]", head_line]

    warns = []
    if unmerged:
        warns.append("⚠️ **머지 안 된** agent 브랜치 "
                     f"{len(unmerged)}개: " + ", ".join(f"`{b}`" for b in unmerged))
    if merged_leftover:
        warns.append(f"· 머지됐으나 미삭제 agent 브랜치 {len(merged_leftover)}개: "
                     + ", ".join(f"`{b}`" for b in merged_leftover))
    if extra_worktrees:
        warns.append(f"⚠️ 활성 worktree {len(extra_worktrees)}개 — 종료 후 `git worktree remove` 필요 가능")

    if warns:
        out += ["- " + w for w in warns]
        out.append("  → drift 점검: `git branch --list 'agent/*'` / `git worktree list` "
                   "(AGENT_STATUS.md 표는 수동이라 git 이 진실)")
    else:
        out.append("- 멀티에이전트 상태 clean (잔존 agent 브랜치/worktree 없음)")

    # Windows 훅 캡처 코덱(cp949)과 무관하게 UTF-8 바이트로 직접 출력 → 한글 mojibake 방지
    # (check_cpp_invariants.py 의 _emit 과 동일 패턴)
    sys.stdout.buffer.write(("\n".join(out) + "\n").encode("utf-8"))
    sys.stdout.buffer.flush()
    sys.exit(0)


if __name__ == "__main__":
    main()
