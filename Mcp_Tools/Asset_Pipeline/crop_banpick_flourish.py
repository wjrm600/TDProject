#!/usr/bin/env python3
"""crop_banpick_flourish.py — ComfyUI 생성본에서 벤픽 제목 날개 장식 추출 (white-on-alpha).

- T_BanPick_WingSide ← T_BanPick_WingL_A_gen.png 중앙 날개쌍의 **왼쪽 날개 단품**
  (위젯이 제목 양옆에 좌=원본 / 우=RenderScale X=-1 미러로 배치)

처리 파이프라인:
1) 창 크롭: 생성 이미지의 모서리 프레임 장식을 1차 배제
2) 알파 레벨(임계값): 배경 노이즈 → 0 (희미한 사각 박스 방지)
3) **최대 연결 성분만 유지**: 창에 남은 떠다니는 장식 조각 제거 (날개 = 최대 덩어리)
4) 콘텐츠 bbox 크롭(+여백): 잘림 0

시드 의존 — 재생성 시 창 값 재조정.
  python crop_banpick_flourish.py
"""
import os
import sys
from collections import deque

from PIL import Image

for _s in (sys.stdout, sys.stderr):
    try:
        _s.reconfigure(encoding="utf-8")
    except Exception:
        pass

RAW = os.path.join(os.path.dirname(os.path.abspath(__file__)), "RawAssets")
BLACK, WHITE = 48, 215   # 알파 레벨 (배경 노이즈 컷 / 라인 알파 풀)
MARGIN = 10              # bbox 크롭 여백(px)


def alpha_levels(lum: Image.Image) -> Image.Image:
    return lum.point(lambda v: 0 if v <= BLACK else (255 if v >= WHITE
                     else int((v - BLACK) * 255 / (WHITE - BLACK))))


def keep_largest_component(alpha: Image.Image) -> Image.Image:
    """알파>0 픽셀의 8-연결 성분 중 최대만 유지 (떠다니는 조각 제거)."""
    w, h = alpha.size
    px = alpha.load()
    label = [[0] * w for _ in range(h)]
    sizes = {}
    cur = 0
    for sy in range(h):
        for sx in range(w):
            if px[sx, sy] > 0 and label[sy][sx] == 0:
                cur += 1
                n = 0
                q = deque([(sx, sy)])
                label[sy][sx] = cur
                while q:
                    x, y = q.popleft()
                    n += 1
                    for dy in (-1, 0, 1):
                        for dx in (-1, 0, 1):
                            nx, ny = x + dx, y + dy
                            if 0 <= nx < w and 0 <= ny < h and px[nx, ny] > 0 and label[ny][nx] == 0:
                                label[ny][nx] = cur
                                q.append((nx, ny))
                sizes[cur] = n
    if not sizes:
        return alpha
    best = max(sizes, key=sizes.get)
    for y in range(h):
        for x in range(w):
            if px[x, y] > 0 and label[y][x] != best:
                px[x, y] = 0
    return alpha


def save_rgba(alpha: Image.Image, out_name: str):
    white = Image.new("L", alpha.size, 255)
    rgba = Image.merge("RGBA", (white, white, white, alpha))
    out = os.path.join(RAW, out_name)
    rgba.save(out)
    print(f"saved: {out}  ({alpha.size[0]}x{alpha.size[1]})")


def main():
    src = os.path.join(RAW, "T_BanPick_WingL_A_gen.png")
    if not os.path.exists(src):
        print("[오류] 원본 없음:", src)
        sys.exit(1)
    lum = Image.open(src).convert("L")
    W, H = lum.size
    # 중앙 날개쌍의 왼쪽 날개 창 (모서리 프레임 장식 배제)
    win = lum.crop((int(W * 0.13), int(H * 0.06), int(W * 0.495), int(H * 0.88)))
    a = alpha_levels(win)
    a = keep_largest_component(a)
    bbox = a.getbbox()
    if bbox:
        l, t, r, b = bbox
        w2, h2 = a.size
        a = a.crop((max(0, l - MARGIN), max(0, t - MARGIN),
                    min(w2, r + MARGIN), min(h2, b + MARGIN)))
    save_rgba(a, "T_BanPick_WingSide.png")


if __name__ == "__main__":
    main()
