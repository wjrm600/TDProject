#!/usr/bin/env python3
"""make_banpick_backdrop_v2.py — LoL 스타일 다크 판타지 챔피언 선택 배경 고도화판.

기존 make_banpick_backdrop.py 대비 개선:
  - 더 깊은 암흑감: 3중 비네트(방사형 + 타원형 + 모서리)
  - 중앙 은은한 글로우(캐릭터 그리드 영역이 살짝 밝게 — UI 텍스트 영역은 어두움)
  - 미세한 대각 광원 스트릭(존재감은 있지만 전보다 훨씬 절제)
  - 전체적으로 30% 더 어둡게: UI 상단/하단 텍스트 가독성 확보
  - 채도 거의 없는 청회색 팔레트 (콜드 미스트 느낌)
  - 필름 그레인(salt-and-pepper) + 가우시안 블러로 고급스러운 텍스처감

출력: RawAssets/T_BanPick_Backdrop.png (RGB, 1920x1080)
"""

import os
import math
import random
from PIL import Image, ImageFilter

W, H = 1920, 1080

# ── 팔레트 (모두 이전보다 20-30 포인트씩 낮춤) ──────────────────────────────
BASE_EDGE   = (2,   3,   6)      # 모서리 최어두움
BASE_MID    = (10,  13,  22)     # 중간 영역
CENTER_GLOW = (20,  25,  42)     # 중앙 캐릭터 그리드 뒤쪽 미세 글로우
STREAK_COL  = (28,  40,  68)     # 대각 스트릭 (이전보다 어둡게)

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
RAW_DIR    = os.path.join(SCRIPT_DIR, "RawAssets")
OUT_PATH   = os.path.join(RAW_DIR, "T_BanPick_Backdrop.png")


def lerp(a, b, t):
    t = max(0.0, min(1.0, t))
    return a + (b - a) * t


def lerp_color(a, b, t):
    return tuple(int(lerp(a[i], b[i], t)) for i in range(3))


def clamp(v, lo=0, hi=255):
    return max(lo, min(hi, v))


def main():
    os.makedirs(RAW_DIR, exist_ok=True)
    rng = random.Random(2026)

    cx, cy = W / 2.0, H / 2.0
    # 방사형 정규화 기준: 모서리까지 거리
    max_diag = math.sqrt(cx**2 + cy**2)

    # ── 1. 베이스 레이어: 방사형 그라데이션 (중앙 글로우 + 가장자리 어둠) ──────
    print("[1/6] 베이스 방사형 그라데이션 생성...")
    base = Image.new("RGB", (W, H))
    px = base.load()

    for y in range(H):
        for x in range(W):
            dx, dy = x - cx, y - cy

            # 방사형 거리 (0=중앙, 1=모서리)
            r_radial = math.sqrt(dx*dx + dy*dy) / max_diag

            # 수직 방향 그라데이션 (상단/하단 더 어둡게)
            r_vert = abs(y - cy) / cy   # 0=중앙, 1=상단/하단 끝

            # 합성 어둠 계수: 방사형 70% + 수직 30%
            t = r_radial ** 1.6 * 0.7 + r_vert ** 2.0 * 0.3
            t = min(t, 1.0)

            color = lerp_color(CENTER_GLOW, BASE_EDGE, t)
            px[x, y] = color

    # ── 2. 타원형 글로우 오버레이 (캐릭터 그리드 영역, 화면 중앙 60%×40%) ──────
    print("[2/6] 중앙 글로우 오버레이...")
    # 타원 반경
    ellipse_rx = W * 0.30   # 수평 반경 (화면 너비의 30%)
    ellipse_ry = H * 0.20   # 수직 반경 (화면 높이의 20%)
    GLOW_COLOR = (30, 38, 62)  # 글로우 밝기 (BASE_MID 보다 약간 밝음)

    glow_arr = base.load()
    for y in range(H):
        for x in range(W):
            dx, dy = x - cx, y - cy
            # 타원 거리 (0=중심, 1=타원 테두리, >1=바깥)
            e_dist = math.sqrt((dx / ellipse_rx)**2 + (dy / ellipse_ry)**2)
            if e_dist < 1.5:
                # 0→1.5 사이에서 선형 페이드
                t_glow = 1.0 - min(e_dist / 1.5, 1.0)
                t_glow = t_glow ** 2.0  # 부드러운 폴오프
                # 현재 색상과 블렌드
                curr = glow_arr[x, y]
                blended = lerp_color(curr, GLOW_COLOR, t_glow * 0.45)
                glow_arr[x, y] = blended

    # ── 3. 대각 광원 스트릭 (절제된 2개만) ──────────────────────────────────────
    print("[3/6] 대각 스트릭 추가...")
    streak_params = [
        # (offset, thickness, opacity)
        (-180, 140, 0.06),   # 메인 스트릭 (은은하게)
        (620,  80,  0.04),   # 보조 스트릭
    ]
    for offset, thickness, opacity in streak_params:
        streak_mask = Image.new("L", (W, H), 0)
        sm = streak_mask.load()
        half = thickness / 2.0
        for y in range(H):
            for x in range(W):
                dist = abs((x - y - offset) / math.sqrt(2))
                if dist < half:
                    fade = 1.0 - (dist / half) ** 2.0
                    sm[x, y] = int(255 * fade * opacity)
        streak_color_img = Image.new("RGB", (W, H), STREAK_COL)
        base = Image.composite(streak_color_img, base, streak_mask)

    # ── 4. 가우시안 블러 (전체 소프트닝) ────────────────────────────────────────
    print("[4/6] 가우시안 블러...")
    base = base.filter(ImageFilter.GaussianBlur(radius=4))

    # ── 5. 3중 비네트 (매우 강한 어두움, 가장자리 완전 블랙에 가깝게) ──────────
    print("[5/6] 강한 3중 비네트...")

    # 비네트 1: 방사형 (기본 어두움)
    v_arr = base.load()
    for y in range(H):
        for x in range(W):
            dx, dy = x - cx, y - cy
            r = math.sqrt(dx*dx + dy*dy) / max_diag
            darkness = r ** 1.8  # 0=중앙(없음), 1=모서리(최대)
            factor = 1.0 - darkness * 0.75  # 최대 75% 어둡게
            r_, g_, b_ = v_arr[x, y]
            v_arr[x, y] = (
                clamp(int(r_ * factor)),
                clamp(int(g_ * factor)),
                clamp(int(b_ * factor)),
            )

    # 비네트 2: 모서리 추가 다크닝 (코너 크러시)
    for y in range(H):
        for x in range(W):
            # 각 모서리까지 거리 중 최소값
            corner_t_x = abs(x - cx) / cx   # 0=중앙, 1=좌우 끝
            corner_t_y = abs(y - cy) / cy   # 0=중앙, 1=상하 끝
            # 모서리 효과: 두 축 모두 크면 더 강하게
            corner_factor = (corner_t_x * corner_t_y) ** 1.2
            factor = 1.0 - corner_factor * 0.55
            r_, g_, b_ = v_arr[x, y]
            v_arr[x, y] = (
                clamp(int(r_ * factor)),
                clamp(int(g_ * factor)),
                clamp(int(b_ * factor)),
            )

    # 비네트 3: 전체 밝기를 0.75배로 (전반적 어둡힘 — UI 텍스트 가독성)
    for y in range(H):
        for x in range(W):
            r_, g_, b_ = v_arr[x, y]
            v_arr[x, y] = (
                clamp(int(r_ * 0.76)),
                clamp(int(g_ * 0.76)),
                clamp(int(b_ * 0.76)),
            )

    # ── 6. 필름 그레인 (아주 미세하게) ─────────────────────────────────────────
    print("[6/6] 필름 그레인...")
    px2 = base.load()
    for y in range(H):
        for x in range(W):
            n = rng.randint(-5, 5)
            r_, g_, b_ = px2[x, y]
            px2[x, y] = (clamp(r_ + n), clamp(g_ + n), clamp(b_ + n))

    # ── 저장 ────────────────────────────────────────────────────────────────────
    base.save(OUT_PATH, "PNG")
    size_kb = os.path.getsize(OUT_PATH) // 1024
    print(f"\n저장 완료: {OUT_PATH}")
    print(f"크기: {W}x{H}  ({size_kb} KB)")


if __name__ == "__main__":
    main()
