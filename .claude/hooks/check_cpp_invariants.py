#!/usr/bin/env python3
"""
AI 하네스 (d): C++ 불변식 가드레일 — PostToolUse 훅.

Edit/Write/MultiEdit 직후 자동 실행되어, 방금 편집한 .h/.cpp 가 CLAUDE.md 의
핵심 불변식(메모리/DS 안전)을 어겼는지 휴리스틱으로 검사하고, 의심되면 에이전트에게
경고를 되먹인다(stderr + exit 2). **차단이 아니라 넛지** — false positive 가능하므로
에이전트가 읽고 판단(수정 or 무시)한다. 사람의 수동 에디터 편집에는 발동하지 않음
(Claude Code 의 도구 호출에만 발동).

검사 (CLAUDE.md "Memory Management" / "Dedicated Server 환경"):
  [.h]   1. TArray<U*/A*> 멤버에 UPROPERTY() 누락 → GC 크래시
  [.cpp] 2. CreateWidget/AddToViewport 인데 IsLocalPlayerController 가드가 파일에 없음 → DS 위젯 크래시
  [.cpp] 3. SpawnActor/Destroy() 인데 HasAuthority 가 파일에 없음 → 서버 권한 누락 (저신뢰)

수동 테스트:
  echo '{"tool_name":"Write","tool_input":{"file_path":"X.h"}}' | python .claude/hooks/check_cpp_invariants.py
"""
import sys
import os
import re
import json


def main():
    try:
        # 콘솔 코드페이지(예: cp949)와 무관하게 UTF-8 로 읽는다 (경로에 한글/공백 안전).
        data = json.loads(sys.stdin.buffer.read().decode("utf-8", "replace"))
    except Exception:
        sys.exit(0)  # 입력 파싱 실패 → 조용히 통과 (훅이 작업을 막지 않게)

    if data.get("tool_name") not in ("Edit", "Write", "MultiEdit"):
        sys.exit(0)

    fp = (data.get("tool_input") or {}).get("file_path", "")
    if not fp:
        sys.exit(0)

    norm = fp.replace("\\", "/")
    ext = os.path.splitext(norm)[1].lower()
    if ext not in (".h", ".hpp", ".cpp", ".cc"):
        sys.exit(0)
    # 생성/서드파티/엔진 파일 제외
    if "/Intermediate/" in norm or "/ThirdParty/" in norm or norm.endswith(".generated.h"):
        sys.exit(0)

    try:
        with open(fp, "r", encoding="utf-8", errors="ignore") as f:
            lines = f.read().splitlines()
    except Exception:
        sys.exit(0)

    text = "\n".join(lines)
    name = os.path.basename(fp)
    is_header = ext in (".h", ".hpp")
    is_source = ext in (".cpp", ".cc")
    warnings = []

    # ── Check 1 (.h only): UObject* TArray 멤버에 UPROPERTY() 누락 → GC 크래시 ──
    # 멤버 선언은 헤더에만 있으므로 .h 로 한정 → .cpp 의 지역 변수(예: GetOverlappingActors
    # 결과용 TArray<AActor*>)를 오탐하지 않음.
    if is_header:
        decl_re = re.compile(r"^\s*TArray<\s*[AU][A-Za-z0-9_]+\s*\*\s*>\s*[A-Za-z_]\w*\s*;")
        for i, ln in enumerate(lines):
            if decl_re.search(ln):
                prev = " ".join(lines[max(0, i - 2):i])  # 앞 2줄에 UPROPERTY 있는지
                if "UPROPERTY" not in prev:
                    warnings.append(
                        f"{name}:{i+1}  UObject* TArray 멤버에 UPROPERTY() 없음 → GC 크래시 위험\n"
                        f"      > {ln.strip()}"
                    )

    # ── Check 2 (.cpp): CreateWidget/AddToViewport 인데 로컬 PC 가드 없음 → DS 위젯 크래시 ──
    if is_source and re.search(r"\b(CreateWidget|AddToViewport)\b", text):
        if not re.search(r"\bIsLocal(PlayerController|Controller)\b", text):
            warnings.append(
                f"{name}  CreateWidget/AddToViewport 사용 — IsLocalPlayerController() 가드가 "
                f"파일에 안 보임 (DS 위젯 생성 크래시 의심)"
            )

    # ── Check 3 (.cpp, 저신뢰): SpawnActor/Destroy() 인데 HasAuthority 없음 → 서버 권한 누락 ──
    if is_source and re.search(r"\bSpawnActor\b|->\s*Destroy\s*\(", text):
        if not re.search(r"\bHasAuthority\b", text):
            warnings.append(
                f"{name}  SpawnActor/Destroy() 사용 — HasAuthority() 가 파일에 안 보임 "
                f"(서버 전용 권한 누락 의심, 저신뢰)"
            )

    if warnings:
        msg = (
            "⚠️ [가드레일] CLAUDE.md 불변식 위반 의심 (휴리스틱 — false positive 가능, 차단 아님):\n"
            + "\n".join("- " + w for w in warnings)
        )
        # 콘솔 코드페이지 무관하게 UTF-8 바이트로 직접 출력 (cp949 에서 emoji/한글 인코딩 에러 회피).
        sys.stderr.buffer.write((msg + "\n").encode("utf-8"))
        sys.stderr.buffer.flush()
        sys.exit(2)  # PostToolUse: stderr 가 에이전트에게 피드백됨

    sys.exit(0)


if __name__ == "__main__":
    main()
