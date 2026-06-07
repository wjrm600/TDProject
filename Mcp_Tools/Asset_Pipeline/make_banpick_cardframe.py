#!/usr/bin/env python3
"""make_banpick_cardframe.py — 벤픽 카드용 9-slice 가는 골드 프레임 (얇은 렌더 최적화).

핵심: 작은 텍스처(64x64) + 가는 테두리(8px=12.5%) → 위젯이 ImageSize~32 로 렌더하면 ~4px,
다운스케일 2:1 이라 얇아도 또렷(256px/64px 테두리를 4px 로 렌더하면 16:1 → 뭉개짐 문제 해결).

위젯 파라미터(UAOSBanPickWidget): DrawAs=Box, Margin=0.125, ImageSize≈32, 카드 padding≈4.
출력: RawAssets/T_BanPick_CardFrame.png (RGBA 64x64)
임포트: /Game/AOS/UI/Assets/T_BanPick_CardFrame
"""
import os
from PIL import Image, ImageDraw

S = 64
MARGIN = 0.125
BORDER = int(round(S * MARGIN))   # 8px
INNER = S - BORDER                # 56

# 밝은 골드(틴트 잘 먹게)
GOLD_BRIGHT = (255, 240, 185, 255)   # 외곽 하이라이트
GOLD_MID    = (214, 184, 118, 255)   # 베벨 중간
GOLD_DARK   = (150, 118,  60, 255)   # 베벨 안쪽
GOLD_SHADOW = ( 64,  48,  20, 255)   # 내곽 그림자 라인


def lerp(a, b, t):
    return tuple(int(a[i] + (b[i] - a[i]) * t) for i in range(4))


def main():
    img = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)

    # 베벨 테두리: 바깥(밝음) → 안쪽(어둠) 1px 링 누적
    for i in range(BORDER):
        if i == 0:
            c = GOLD_BRIGHT                       # 가장 바깥 = 밝은 하이라이트
        elif i == BORDER - 1:
            c = GOLD_SHADOW                       # 가장 안쪽 = 어두운 경계선
        else:
            c = lerp(GOLD_MID, GOLD_DARK, i / (BORDER - 1))
        d.rectangle([i, i, S - 1 - i, S - 1 - i], outline=c, width=1)

    # 코너 악센트 — 모서리 작은 밝은 점(얇아도 코너가 살짝 강조)
    for (cx, cy) in [(1, 1), (S - 2, 1), (1, S - 2), (S - 2, S - 2)]:
        d.rectangle([cx - 1, cy - 1, cx + 1, cy + 1], fill=GOLD_BRIGHT)

    # 중앙 완전 투명 (초상화가 덮음)
    d.rectangle([BORDER, BORDER, INNER - 1, INNER - 1], fill=(0, 0, 0, 0))

    dst = os.path.join(
        os.path.dirname(os.path.abspath(__file__)),
        "RawAssets", "T_BanPick_CardFrame.png"
    )
    os.makedirs(os.path.dirname(dst), exist_ok=True)
    img.save(dst)
    print(f"saved {dst} ({S}x{S} RGBA)")
    print(f"9-slice border: {BORDER}px / {S}px = {BORDER / S:.4f} ({BORDER / S * 100:.1f}%)")
    print(f"transparent center: {BORDER}~{INNER-1} ({INNER-BORDER}x{INNER-BORDER}px)")


if __name__ == "__main__":
    main()
