#!/usr/bin/env python3
"""
에이전트 Spawn 감사 로그 — PreToolUse(Task) 훅.

목적: `AGENT_STATUS.md` 수동 표가 drift 하는 문제(머지 안 된 브랜치 4개월 방치 사례,
AGENT_STATUS.md line 17-19)를 근본 보완. Task 도구로 서브에이전트가 호출될 때마다
자동으로 [시각 · 에이전트 · 현재 git 브랜치 · 작업 설명]을 audit 로그에 append 한다.
사람이 표를 갱신하길 기다리지 않고 git 만큼이나 신뢰할 수 있는 기록을 남기는 게 핵심.

비차단(non-blocking): 항상 exit 0. Task 가 아니거나 파싱 실패해도 조용히 통과 →
정상 도구 흐름을 절대 막지 않는다.

진입점:
  · 훅 모드 (stdin 으로 PreToolUse JSON): Claude Code 가 Task 도구 실행 직전 자동 호출.
  · CLI 모드 (--show): 누적 로그를 stdout 으로 출력 (빠른 확인용).

수동 테스트:
  훅:  echo '{"tool_name":"Task","tool_input":{"subagent_type":"agent-prog-ai","description":"x"}}' | python .claude/hooks/log_agent_spawn.py
  확인: python .claude/hooks/log_agent_spawn.py --show
"""
import sys
import json
import subprocess
from datetime import datetime
from pathlib import Path

LOG_PATH = Path(__file__).resolve().parent.parent / "coordination" / "AGENT_SPAWN_LOG.md"

LOG_HEADER = (
    "# 에이전트 Spawn 감사 로그 (자동 생성 — 수동 편집 금지)\n\n"
    "> PreToolUse(Task) 훅(`log_agent_spawn.py`)이 자동 기록한다.\n"
    "> `AGENT_STATUS.md`(수동, drift 함)의 보완용 — \"실제로 언제 어떤 에이전트가 어느 브랜치에서\n"
    "> spawn 됐나\"의 신뢰 기록. 권위는 여전히 git 이지만, 이 로그는 표 누락을 역추적하게 해준다.\n\n"
    "| 시각 | 에이전트 | 브랜치 | 작업 설명 |\n"
    "|------|---------|--------|-----------|\n"
)


def current_branch():
    """현재 git 브랜치 (drift 추적의 핵심 — 어느 브랜치 컨텍스트에서 spawn 됐나)."""
    try:
        out = subprocess.run(
            ["git", "rev-parse", "--abbrev-ref", "HEAD"],
            capture_output=True, text=True, timeout=5,
        )
        return out.stdout.strip() or "(detached)"
    except Exception:
        return "(no-git)"


def show():
    if LOG_PATH.exists():
        sys.stdout.write(LOG_PATH.read_text(encoding="utf-8"))
    else:
        sys.stdout.write("(로그 없음 — 아직 Task spawn 기록 없음)\n")
    sys.exit(0)


def main():
    if "--show" in sys.argv[1:]:
        show()

    try:
        payload = json.load(sys.stdin)
    except Exception:
        sys.exit(0)  # 파싱 실패해도 도구 흐름 막지 않음

    if payload.get("tool_name") != "Task":
        sys.exit(0)

    tin = payload.get("tool_input", {}) or {}
    subagent = tin.get("subagent_type") or "(default)"
    desc = (tin.get("description") or "").replace("\n", " ").replace("|", "/").strip()
    if len(desc) > 120:
        desc = desc[:117] + "..."

    ts = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    line = f"| {ts} | `{subagent}` | {current_branch()} | {desc} |\n"

    try:
        if not LOG_PATH.exists():
            LOG_PATH.write_text(LOG_HEADER, encoding="utf-8")
        with LOG_PATH.open("a", encoding="utf-8") as f:
            f.write(line)
    except Exception:
        pass  # 로깅 실패가 spawn 을 막아선 안 됨

    sys.exit(0)


if __name__ == "__main__":
    main()
