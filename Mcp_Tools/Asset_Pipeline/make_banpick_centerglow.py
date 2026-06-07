#!/usr/bin/env python3
"""make_banpick_centerglow.py — 벤픽 중앙 비네팅 글로우 텍스처 (방사형 소프트 글로우).

그리드 뒤에 깔아 중앙을 살짝 밝게(시선 집중). 따뜻한 화이트, 가장자리 완전 투명.
위젯이 RootOverlay 중앙에 HitTestInvisible 로 배치(SetDesiredSizeOverride 로 크기 지정).
출력: RawAssets/T_BanPick_CenterGlow.png (RGBA 256x256)
임포트: /Game/AOS/UI/Assets/T_BanPick_CenterGlow
"""
import os
import math
from PIL import Image

S = 256
PEAK_ALPHA = 0.62          # 중앙 최대 불투명도 (과감하게)
COL = (255, 220, 140)      # 따뜻한 골드(글로우)
FALLOFF = 1.30             # 작을수록 넓게 퍼짐


def main():
    img = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    px = img.load()
    c = (S - 1) / 2.0
    maxr = S / 2.0
    for y in range(S):
        for x in range(S):
            dx = (x - c) / maxr
            dy = (y - c) / maxr
            d = math.sqrt(dx * dx + dy * dy)
            if d >= 1.0:
                a = 0.0
            else:
                t = 1.0 - d
                a = PEAK_ALPHA * (t ** FALLOFF)   # falloff (작을수록 넓게 퍼짐)
            px[x, y] = (COL[0], COL[1], COL[2], int(a * 255))

    dst = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                       "RawAssets", "T_BanPick_CenterGlow.png")
    os.makedirs(os.path.dirname(dst), exist_ok=True)
    img.save(dst)
    print(f"saved {dst} ({S}x{S} RGBA, peak alpha {PEAK_ALPHA})")


if __name__ == "__main__":
    main()
