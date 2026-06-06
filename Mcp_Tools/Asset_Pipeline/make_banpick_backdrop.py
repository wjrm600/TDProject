#!/usr/bin/env python3
"""make_banpick_backdrop.py — LoL 챔피언 선택 화면 스타일 다크 백드롭 절차 생성.

출력: RawAssets/T_BanPick_Backdrop.png (RGB, 1920x1080)
언리얼 임포트 대상: /Game/AOS/UI/Assets/T_BanPick_Backdrop

특징:
  - 딥 네이비/차콜 베이스
  - 중앙 밝은 radial vignette, 가장자리 어두운 비네트
  - 옅은 대각선 광원 스트릭 (좌상→우하)
  - 채도 낮은 냉색 팔레트 (LoL 픽창 분위기)
"""
import os
import math
from PIL import Image, ImageDraw, ImageFilter

W, H = 1920, 1080

# 색상 팔레트
BASE_DARK   = (8,  10, 18)      # 배경 최어두움 (거의 블랙 네이비)
CENTER_MID  = (18, 22, 38)      # 중앙 살짝 밝음
VIGNETTE    = (4,  5,  9)       # 가장자리 완전 어두움
STREAK_COL  = (35, 50, 80)      # 대각 광원 스트릭 색

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
RAW_DIR    = os.path.join(SCRIPT_DIR, "RawAssets")


def lerp_color(a, b, t):
    t = max(0.0, min(1.0, t))
    return tuple(int(a[i] + (b[i] - a[i]) * t) for i in range(3))


def main():
    os.makedirs(RAW_DIR, exist_ok=True)

    # ── 1. 베이스 레이어: 픽셀별 radial 그라데이션 ──────────────────────────────
    # 중앙은 살짝 밝고 가장자리는 매우 어두운 코스믹 비네트
    base_img = Image.new("RGB", (W, H), BASE_DARK)
    base_arr = base_img.load()
    cx, cy = W / 2, H / 2
    # 모서리까지의 최대 거리 (정규화 기준)
    max_dist = math.sqrt(cx**2 + cy**2)

    for y in range(H):
        for x in range(W):
            dist = math.sqrt((x - cx)**2 + (y - cy)**2)
            t_radial = dist / max_dist  # 0(중앙) ~ 1(모서리)

            # 중앙 살짝 밝음 → 가장자리 어두움
            t_vignette = t_radial ** 1.4   # 비선형 (가장자리 급격히 어두워짐)
            color = lerp_color(CENTER_MID, VIGNETTE, t_vignette)
            base_arr[x, y] = color

    # ── 2. 대각 광원 스트릭 레이어 ───────────────────────────────────────────────
    # 좌상단 → 우하단 방향의 부드러운 빛 띠 (여러 개, 불규칙 간격)
    streak_layer = Image.new("RGB", (W, H), (0, 0, 0))
    sd = ImageDraw.Draw(streak_layer)

    # 대각선 방향 (45도): 여러 개의 스트릭
    streak_params = [
        # (중심 offset, 두께, 불투명도 비율)
        (-300, 250, 0.18),
        (100,  180, 0.12),
        (500,  120, 0.07),
        (800,  350, 0.10),
        (1200, 90,  0.05),
    ]
    for offset, thickness, opacity in streak_params:
        # 대각선 방향으로 기울어진 평행사변형 그리기
        # 45도 기울기 직선에서 offset 만큼 수직 이동한 띠
        # 직선: x - y = offset  →  각 픽셀에서 거리 계산
        streak_mask = Image.new("L", (W, H), 0)
        sm = streak_mask.load()
        half = thickness / 2.0
        for y in range(H):
            for x in range(W):
                signed_dist = (x - y - offset) / math.sqrt(2)
                abs_dist = abs(signed_dist)
                if abs_dist < half:
                    # 가장자리로 갈수록 페이드
                    fade = 1.0 - (abs_dist / half) ** 1.5
                    sm[x, y] = int(255 * fade * opacity)
        streak_color_img = Image.new("RGB", (W, H), STREAK_COL)
        base_img = Image.composite(streak_color_img, base_img, streak_mask)

    # ── 3. 가우시안 블러로 부드럽게 ─────────────────────────────────────────────
    base_img = base_img.filter(ImageFilter.GaussianBlur(radius=3))

    # ── 4. 2차 비네트 오버레이 (진한 어두움, 곱셈 효과) ──────────────────────────
    vignette2 = Image.new("RGB", (W, H), (0, 0, 0))
    v2_arr = vignette2.load()
    for y in range(H):
        for x in range(W):
            dist = math.sqrt((x - cx)**2 + (y - cy)**2)
            t = (dist / max_dist) ** 2.2
            alpha = int(t * 180)   # 최대 180/255 어두움
            v2_arr[x, y] = (alpha, alpha, alpha)

    # Screen blend 반전 (어두워지는 효과: base * (1 - vignette/255))
    vignette_mask = vignette2.convert("L")
    black_layer = Image.new("RGB", (W, H), (0, 0, 0))
    base_img = Image.composite(black_layer, base_img, vignette_mask)

    # ── 5. 미세 노이즈로 필름 그레인 느낌 ────────────────────────────────────────
    import random
    rng = random.Random(42)
    noise_layer = Image.new("RGB", (W, H), (0, 0, 0))
    nl = noise_layer.load()
    bi = base_img.load()
    for y in range(H):
        for x in range(W):
            n = rng.randint(-6, 6)
            r, g, b = bi[x, y]
            nl[x, y] = (max(0, min(255, r + n)),
                         max(0, min(255, g + n)),
                         max(0, min(255, b + n)))
    base_img = noise_layer

    # ── 저장 ─────────────────────────────────────────────────────────────────────
    out_path = os.path.join(RAW_DIR, "T_BanPick_Backdrop.png")
    base_img.save(out_path, "PNG")
    print(f"saved: {out_path}")
    print(f"size:  {W}x{H}  ({os.path.getsize(out_path)//1024} KB)")


if __name__ == "__main__":
    main()
