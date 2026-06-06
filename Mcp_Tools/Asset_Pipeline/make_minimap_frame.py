#!/usr/bin/env python3
"""make_minimap_frame.py — 미니맵 SF 베젤 테두리 텍스처 절차 생성 (가운데 투명).

출력: RawAssets/T_MinimapFrame.png (RGBA).
언리얼 임포트: /Game/AOS/UI/Assets/T_MinimapFrame  (UAOSMinimapWidget 가 자동 로드).

특징: muted 청록 솔리드 밴드 + 바깥 밝음→안쪽 어둠 그라데이션(베젤 입체감) + 글로우.
파라미터(OB/BAND/색)만 바꿔 두께·색·테마 조정 가능. AI 생성과 달리 장식 없이 정확한 솔리드 밴드.
"""
import os
from PIL import Image, ImageDraw, ImageFilter

S = 1024
OB = 8           # 바깥 여백(작을수록 패널 끝에 flush)
BAND = 51        # 밴드 두께 (≈13px @ 260 미니맵)
OUTER = (105, 190, 196)   # 바깥(밝은 muted 청록)
INNER = (34, 86, 96)      # 안쪽(어둠) — 그라데이션
EDGE = (165, 228, 232)    # 또렷한 외곽선
HOLE = 95        # 이 안쪽은 완전 투명(맵 또렷)


def _lerp(a, b, t):
    return tuple(int(a[i] + (b[i] - a[i]) * t) for i in range(3))


def main():
    base = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    d = ImageDraw.Draw(base)
    for i in range(BAND):                      # 중첩 사각으로 그라데이션 밴드
        t = i / max(1, BAND - 1)
        d.rectangle([OB + i, OB + i, S - OB - i, S - OB - i],
                    outline=_lerp(OUTER, INNER, t) + (255,), width=2)
    d.rectangle([OB, OB, S - OB, S - OB], outline=EDGE + (255,), width=3)
    out = Image.alpha_composite(base.filter(ImageFilter.GaussianBlur(10)), base)
    ImageDraw.Draw(out).rectangle([HOLE, HOLE, S - HOLE, S - HOLE], fill=(0, 0, 0, 0))
    dst = os.path.join(os.path.dirname(os.path.abspath(__file__)), "RawAssets", "T_MinimapFrame.png")
    os.makedirs(os.path.dirname(dst), exist_ok=True)
    out.save(dst)
    print("saved", dst)


if __name__ == "__main__":
    main()
