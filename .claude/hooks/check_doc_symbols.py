#!/usr/bin/env python3
"""
AI 하네스 가드레일 (문서 drift): 에이전트/조율 문서가 참조하는 C++ 함수 심볼이
실제 Source/ 코드에 존재하는지 검사한다. false positive 가능 → "차단"이 아니라 "넛지".

동기: `.claude/agents/*.md` · `.claude/coordination/*.md` 는 코드가 리팩터되면 조용히 drift 한다
      (예: Phase 6 에서 제거된 UpdateAIBehavior/AttackTarget/MoveTowardsTarget 을 계속 "호출한다"
       고 적어둔 채 방치 → 그 정의로 스폰된 서브에이전트가 없는 API 를 부활시키려 함).
      이 검사는 문서 속 `함수명(` 참조를 Source 코드(주석/문자열 제거) 토큰과 대조해 "실존하지 않는
      심볼"을 표면화한다.

검사 대상 문서: `.claude/agents/*.md`, `.claude/coordination/*.md`
검사 방법:
  1. Source/**/*.{h,hpp,cpp,cc} 를 읽어 블록주석 /*...*/ · 문자열 "..." · 라인주석 //... 을 제거한
     "코드 텍스트"에서 식별자 토큰 집합을 만든다.
     (주석/문자열을 지우는 이유: "// UpdateAIBehavior 제거" 같은 흔적이 심볼을 되살아있게 오판시킴)
  2. 문서에서 코드 컨텍스트(백틱 인라인/```펜스)의 `\b[A-Z][A-Za-z0-9]{3,}\s*\(` 을 후보 함수로 추출.
  3. 후보가 Source 토큰 집합에 없고, denylist 도 아니고, 그 줄이 "제거/deprecated/구/없음" 등
     의도적 부재 표기가 아니면 → drift 의심 경고.

진입점 (check_cpp_invariants.py 와 동일 패턴):
  · 훅 모드(인자 없음, stdin=PostToolUse JSON): Edit/Write 대상이 검사 문서면 그 1개만 검사.
    위반 시 stderr + exit 2 로 에이전트에 피드백.
  · CLI 모드: `--all` → 모든 검사 문서 스캔(CI 용). 파일 경로 인자 → 그 파일들만. 위반 시 stdout + exit 1.

수동 테스트:
  훅:  echo '{"tool_name":"Write","tool_input":{"file_path":".claude/agents/agent-prog-ai.md"}}' | python .claude/hooks/check_doc_symbols.py
  CLI: python .claude/hooks/check_doc_symbols.py --all
"""
import sys
import os
import re
import json
import glob

HEADER = "⚠️ [가드레일] 문서→코드 심볼 drift 의심 (휴리스틱 — false positive 가능):"

# 프로젝트 루트: Claude Code 가 훅에 주입하는 CLAUDE_PROJECT_DIR 우선 → 없으면 스크립트 위치
#   기준(.claude/hooks/ 의 2단계 위). 둘 다 cwd 에 의존하지 않음 (다른 훅의 상대경로 cwd 버그 회피).
PROJECT_ROOT = os.environ.get("CLAUDE_PROJECT_DIR") or os.path.abspath(
    os.path.join(os.path.dirname(__file__), "..", "..")
)
SOURCE_ROOT = os.path.join(PROJECT_ROOT, "Source")

# 검사 대상 문서 (프로젝트 루트 상대 glob)
DOC_GLOBS = [
    ".claude/agents/*.md",
    ".claude/coordination/*.md",
]

# 그 줄이 "의도적 부재(제거/금지/구API)" 표기면 후보를 건너뛴다 → 교정 문서 자기오탐 방지.
#   예: "구 `UpdateAIBehavior()` 은 제거됨 — 부활 금지" 같은 경고 문장.
SKIP_LINE_MARKERS = [
    "제거", "폐기", "삭제", "금지", "deprecated", "removed", "구 ", "구api", "레거시", "legacy",
    "없음", "존재하지", "대체", "not exist", "부활", "안티패턴", "obsolete",
]

# 함수 후보지만 검사에서 제외 (프로젝트 코드엔 없어도 정상인 것들: 순수 개념/외부 흔한 심볼).
#   대부분의 엔진 호출은 프로젝트가 실제로 호출하므로 Source 토큰에 잡혀 통과한다 → denylist 최소 유지.
DENYLIST = {
    # 여기에 "코드엔 없지만 문서 표현상 괜찮은 함수 후보" 를 필요 시 추가.
}

# 예시/플레이스홀더 함수명 조각 → 실심볼 아님, 검사 제외 (예: `UpdateXxx()`, `DoFooBar()`).
PLACEHOLDER_FRAGMENTS = ("Xxx", "Yyy", "Zzz", "Foo", "Bar", "Baz", "Example", "Placeholder", "TODO")


def _strip_code(text):
    """C++ 소스에서 블록주석·문자열·라인주석을 제거해 '실코드'만 남긴다."""
    text = re.sub(r"/\*.*?\*/", " ", text, flags=re.DOTALL)   # 블록주석
    text = re.sub(r'"(?:\\.|[^"\\])*"', " ", text)             # 문자열 리터럴
    text = "\n".join(ln.split("//", 1)[0] for ln in text.splitlines())  # 라인주석
    return text


def build_source_symbols(source_root):
    """Source/ 의 .h/.cpp 코드 토큰(식별자) 집합. 열기 실패/비대상은 건너뜀."""
    symbols = set()
    if not os.path.isdir(source_root):
        return symbols
    tok = re.compile(r"[A-Za-z_][A-Za-z0-9_]*")
    for root, _dirs, files in os.walk(source_root):
        norm = root.replace("\\", "/")
        if "/Intermediate/" in norm or "/ThirdParty/" in norm:
            continue
        for fn in files:
            ext = os.path.splitext(fn)[1].lower()
            if ext not in (".h", ".hpp", ".cpp", ".cc"):
                continue
            try:
                with open(os.path.join(root, fn), "r", encoding="utf-8", errors="ignore") as f:
                    code = _strip_code(f.read())
            except Exception:
                continue
            symbols.update(tok.findall(code))
    return symbols


# 코드 컨텍스트 안의 `함수명(` 후보. 대문자 시작 + 3자 이상 + 공백 없이 바로 여는 괄호.
#   공백 불허가 핵심 필터: 자연어 "Setter (AOSGameMode…)" · "Blueprint Override (Designer)" 처럼
#   괄호 앞 공백이 있는 표현은 함수 호출이 아니라 영어 명사+부연 → 오탐 제거.
_CAND_RE = re.compile(r"\b([A-Z][A-Za-z0-9]{3,})\(")


def _code_spans_of_line(line, in_fence):
    """그 줄에서 '코드'로 취급할 텍스트 조각들을 반환 (펜스 안이면 전체, 아니면 백틱 인라인만)."""
    if in_fence:
        return [line]
    return re.findall(r"`([^`]+)`", line)


def check_doc(fp, symbols):
    """검사 문서 1개 → drift 의심 경고 리스트. 비대상/열기 실패면 []."""
    norm = fp.replace("\\", "/")
    if not norm.endswith(".md"):
        return []
    if not (("/.claude/agents/" in norm or norm.startswith(".claude/agents/") or "\\.claude\\agents\\" in fp)
            or ("/.claude/coordination/" in norm or norm.startswith(".claude/coordination/") or "\\.claude\\coordination\\" in fp)):
        return []
    try:
        with open(fp, "r", encoding="utf-8", errors="ignore") as f:
            lines = f.read().splitlines()
    except Exception:
        return []

    name = os.path.basename(fp)
    warnings = []
    seen = set()
    in_fence = False
    for i, line in enumerate(lines):
        stripped = line.lstrip()
        if stripped.startswith("```"):
            in_fence = not in_fence
            continue
        low = line.lower()
        if any(m in low for m in SKIP_LINE_MARKERS):
            continue
        for span in _code_spans_of_line(line, in_fence):
            for cand in _CAND_RE.findall(span):
                if cand in symbols or cand in DENYLIST or cand in seen:
                    continue
                if any(frag in cand for frag in PLACEHOLDER_FRAGMENTS):
                    continue
                seen.add(cand)
                warnings.append(
                    f"{name}:{i+1}  `{cand}()` 가 Source/ 코드에 없음 → 제거/이름변경된 심볼 참조 의심\n"
                    f"      > {line.strip()[:120]}"
                )
    return warnings


def _emit(stream, warnings):
    msg = HEADER + "\n" + "\n".join("- " + w for w in warnings) + "\n"
    stream.buffer.write(msg.encode("utf-8"))
    stream.buffer.flush()


def _target_docs():
    out = []
    for g in DOC_GLOBS:
        out += glob.glob(os.path.join(PROJECT_ROOT, g))
    return sorted(out)


def run_hook():
    """훅 모드: PostToolUse JSON → 편집 문서 1개 검사 → 위반 시 stderr+exit2."""
    try:
        data = json.loads(sys.stdin.buffer.read().decode("utf-8", "replace"))
    except Exception:
        sys.exit(0)
    if data.get("tool_name") not in ("Edit", "Write", "MultiEdit"):
        sys.exit(0)
    fp = (data.get("tool_input") or {}).get("file_path", "")
    if not fp:
        sys.exit(0)
    symbols = build_source_symbols(SOURCE_ROOT)
    if not symbols:
        sys.exit(0)  # Source 를 못 읽으면 조용히 통과 (검사 무의미)
    warnings = check_doc(fp, symbols)
    if warnings:
        _emit(sys.stderr, warnings)
        sys.exit(2)
    sys.exit(0)


def run_cli(paths):
    """CLI/CI 모드: 문서들 검사 → 위반 시 stdout+exit1, 없으면 exit0."""
    symbols = build_source_symbols(SOURCE_ROOT)
    if not symbols:
        sys.stdout.buffer.write("[가드레일] Source/ 심볼을 읽지 못함 — 문서 심볼 검사 스킵\n".encode("utf-8"))
        sys.exit(0)
    all_warnings = []
    for p in paths:
        all_warnings += check_doc(p, symbols)
    if all_warnings:
        _emit(sys.stdout, all_warnings)
        sys.exit(1)
    sys.stdout.buffer.write("[가드레일] 문서→코드 심볼 검사 통과 — drift 없음\n".encode("utf-8"))
    sys.exit(0)


def main():
    args = sys.argv[1:]
    if "--all" in args:
        run_cli(_target_docs())
        return
    file_args = [a for a in args if not a.startswith("-")]
    if file_args:
        run_cli(file_args)
    else:
        run_hook()


if __name__ == "__main__":
    main()
