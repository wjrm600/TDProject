#!/usr/bin/env python3
"""make_banpick_linetaper.py — 벤픽 장식용 테이퍼 라인 스트립 (PIL 절차생성).

세로 64x1024, 흰색 + 알파. 중앙 코어 라인(샤프) + 은은한 글로우 헤일로,
양 끝 15% 알파 테이퍼(페이드아웃). 위젯에서 SetColorAndOpacity 로 팀색/레드 틴트.
"반듯하고 깨끗한" 선 — AI 생성보다 해석적 계산이 정확.

  python make_banpick_linetaper.py
→ RawAssets/T_BanPick_LineTaper.png
"""
import math
import os
import sys

from PIL import Image

for _s in (sys.stdout, sys.stderr):
    try:
        _s.reconfigure(encoding="utf-8")
    except Exception:
        pass

RAW = os.path.join(os.path.dirname(os.path.abspath(__file__)), "RawAssets")

W, H = 64, 1024
CX = W / 2.0
CORE_HW = 3.0      # 코어 반폭(px) — 풀 알파
GLOW_HW = 16.0     # 글로우 반폭(px) — 코어 밖에서 0 까지 감쇠
GLOW_PEAK = 0.35   # 글로우 시작 알파 (코어 경계에서)
TAPER = 0.15       # 양 끝 페이드 구간 (높이 비율)


def smoothstep(t: float) -> float:
    t = max(0.0, min(1.0, t))
    return t * t * (3.0 - 2.0 * t)


def main():
    os.makedirs(RAW, exist_ok=True)
    img = Image.new("RGBA", (W, H), (255, 255, 255, 0))
    px = img.load()

    taper_px = H * TAPER
    for y in range(H):
        # 세로 테이퍼: 양 끝 0 → smoothstep → 중앙 1
        d_end = min(y, H - 1 - y)
        v = smoothstep(d_end / taper_px) if d_end < taper_px else 1.0

        for x in range(W):
            dx = abs((x + 0.5) - CX)
            if dx <= CORE_HW:
                a = 1.0
            elif dx <= GLOW_HW:
                # 코어 밖 글로우: GLOW_PEAK 에서 0 으로 부드럽게
                t = (dx - CORE_HW) / (GLOW_HW - CORE_HW)
                a = GLOW_PEAK * (1.0 - smoothstep(t))
            else:
                a = 0.0
            a *= v
            if a > 0.0:
                px[x, y] = (255, 255, 255, int(round(a * 255)))

    out = os.path.join(RAW, "T_BanPick_LineTaper.png")
    img.save(out)
    print("saved:", out)

    # 가로 변형 (LOCK IN 양쪽 강조선용) — 레이아웃에서 RenderTransform 회전 시 슬롯 크기가
    # 어긋나므로 가로 텍스처를 별도 생성
    out_h = os.path.join(RAW, "T_BanPick_LineTaperH.png")
    img.transpose(Image.ROTATE_90).save(out_h)
    print("saved:", out_h)


if __name__ == "__main__":
    main()
