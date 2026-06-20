#!/usr/bin/env python3
"""
AI 하네스 (d)+(B): C++ 불변식 가드레일 — 단일 검사 로직, 두 진입점.

방금 편집한/변경된 .h/.cpp 가 CLAUDE.md 의 핵심 불변식(메모리/DS 안전)을 어겼는지
휴리스틱으로 검사한다. false positive 가능 → "차단"이 아니라 "넛지/신호".

진입점:
  · 훅 모드 (인자 없음, stdin 으로 PostToolUse JSON): Claude Code 의 Edit/Write/MultiEdit
    직후 자동 실행. 위반 시 stderr + exit 2 로 에이전트에게 피드백. (사람 수동 편집엔 미발동)
  · CLI 모드 (파일 경로 인자): CI(GitHub Actions)가 push 된 변경 .h/.cpp 를 넘겨 호출.
    위반 시 stdout 출력 + exit 1 로 잡(job) 실패. → 사람 수동 편집/다른 기여자까지 push 시점에 포착.
    훅(에이전트 편집만)과 상호보완.

검사 (CLAUDE.md "Memory Management" / "Dedicated Server 환경"):
  [.h]   1. TArray<U*/A*> 멤버에 UPROPERTY() 누락 → GC 크래시
  [.cpp] 2. CreateWidget/AddToViewport 인데 IsLocalPlayerController 가드가 파일에 없음 → DS 위젯 크래시
  [.cpp] 3. SpawnActor/Destroy() 인데 HasAuthority 가 파일에 없음 → 서버 권한 누락 (저신뢰)

수동 테스트:
  훅:  echo '{"tool_name":"Write","tool_input":{"file_path":"X.h"}}' | python .claude/hooks/check_cpp_invariants.py
  CLI: python .claude/hooks/check_cpp_invariants.py path/to/A.h path/to/B.cpp
"""
import sys
import os
import re
import json

HEADER = "⚠️ [가드레일] CLAUDE.md 불변식 위반 의심 (휴리스틱 — false positive 가능):"


def check_file(fp):
    """단일 .h/.cpp 경로를 검사하고 위반 경고 문자열 리스트를 반환. 비대상/열기 실패면 []."""
    norm = fp.replace("\\", "/")
    ext = os.path.splitext(norm)[1].lower()
    if ext not in (".h", ".hpp", ".cpp", ".cc"):
        return []
    # 생성/서드파티/엔진 파일 제외
    if "/Intermediate/" in norm or "/ThirdParty/" in norm or norm.endswith(".generated.h"):
        return []
    try:
        with open(fp, "r", encoding="utf-8", errors="ignore") as f:
            lines = f.read().splitlines()
    except Exception:
        return []

    # 라인 주석(//...) 제거본으로 검사 → "주석 속 언급"으로 인한 false positive 방지.
    #   (간이 strip: 문자열 내부 // 는 드물어 무시 → 놓치면 false negative 라 nudge 로선 안전한 방향.
    #    블록주석 /* */ 은 미처리.) 예: "// CreateWidget 단계에서…" 같은 설명 주석이 check 2 를 오발동시키던 문제.
    lines = [ln.split("//", 1)[0] for ln in lines]
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

    return warnings


def _emit(stream, warnings):
    """경고 리스트를 UTF-8 바이트로 직접 출력 (cp949 콘솔에서 한글/emoji 인코딩 에러 회피)."""
    msg = HEADER + "\n" + "\n".join("- " + w for w in warnings) + "\n"
    stream.buffer.write(msg.encode("utf-8"))
    stream.buffer.flush()


def run_hook():
    """훅 모드: PostToolUse JSON 을 stdin 으로 받아, 편집 파일 1개 검사 → 위반 시 stderr+exit2."""
    try:
        data = json.loads(sys.stdin.buffer.read().decode("utf-8", "replace"))
    except Exception:
        sys.exit(0)  # 입력 파싱 실패 → 조용히 통과 (훅이 작업을 막지 않게)

    if data.get("tool_name") not in ("Edit", "Write", "MultiEdit"):
        sys.exit(0)

    fp = (data.get("tool_input") or {}).get("file_path", "")
    if not fp:
        sys.exit(0)

    warnings = check_file(fp)
    if warnings:
        _emit(sys.stderr, warnings)
        sys.exit(2)  # PostToolUse: stderr 가 에이전트에게 피드백됨
    sys.exit(0)


def run_cli(paths):
    """CLI 모드(CI): 여러 파일 경로를 검사 → 위반 시 stdout+exit1(잡 실패), 없으면 exit0."""
    all_warnings = []
    for p in paths:
        all_warnings += check_file(p)

    if all_warnings:
        _emit(sys.stdout, all_warnings)
        sys.exit(1)

    sys.stdout.buffer.write("[가드레일] C++ 불변식 검사 통과 — 위반 없음\n".encode("utf-8"))
    sys.stdout.buffer.flush()
    sys.exit(0)


def main():
    file_args = [a for a in sys.argv[1:] if not a.startswith("-")]
    if file_args:
        run_cli(file_args)   # 인자 있으면 CI/CLI 모드
    else:
        run_hook()           # 인자 없으면 훅 모드(stdin)


if __name__ == "__main__":
    main()
