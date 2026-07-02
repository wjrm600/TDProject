# -*- coding: utf-8 -*-
"""ui_postprocess.py — UI 텍스처 후처리 (전부 순수 PIL, 결정적).

핵심 원칙:
- 9-slice 패널의 알파는 rembg 금지 (직선/라운드 엣지를 파먹어 slice 가 깨짐)
  → 절차 도면의 지오메트리에서 마스크 유도 (mask_from_control).
- 버튼 3상태는 3회 생성하지 않고 마스터 1장에서 결정적 파생 (상태 간 일관성).
"""
from PIL import Image, ImageDraw, ImageEnhance, ImageOps


def mask_from_rounded_rect(size, radius_frac, inset=2):
    """라운드 사각 알파 마스크 (제어 도면과 동일 지오메트리)."""
    w, h = size
    mask = Image.new("L", size, 0)
    d = ImageDraw.Draw(mask)
    r = int(min(w, h) * radius_frac)
    d.rounded_rectangle([inset, inset, w - inset, h - inset], radius=r, fill=255)
    return mask


def apply_mask(img, mask):
    img = img.convert("RGBA")
    img.putalpha(mask.resize(img.size))
    return img


def symmetrize_x(img):
    """왼쪽 절반을 미러해 좌우 완전 대칭 강제."""
    w, h = img.size
    left = img.crop((0, 0, w // 2, h))
    out = img.copy()
    out.paste(ImageOps.mirror(left), (w - w // 2, 0))
    return out


def validate_nine_slice(img, margin_frac, tolerance=18.0):
    """엣지 밴드(마진 안쪽 경계 스트립)의 행/열 픽셀 분산으로 타일 가능성 검사.
    중앙 스트레치 영역이 균질해야 9-slice 가 왜곡 없이 늘어난다."""
    rgb = img.convert("RGB")
    w, h = rgb.size
    m = int(min(w, h) * margin_frac)
    center = rgb.crop((m, m, w - m, h - m)).resize((32, 32))
    px = list(center.getdata())
    n = len(px)
    means = [sum(c[i] for c in px) / n for i in range(3)]
    var = sum(sum((c[i] - means[i]) ** 2 for i in range(3)) for c in px) / n
    std = var ** 0.5
    return {"pass": std <= tolerance, "center_std": round(std, 1),
            "tolerance": tolerance}


def derive_states(master):
    """버튼 마스터 1장 → (Normal, Hover, Pressed) 결정적 파생."""
    normal = master
    hover = ImageEnhance.Brightness(master).enhance(1.12)
    hover = ImageEnhance.Color(hover).enhance(1.08)
    pressed = ImageEnhance.Brightness(master).enhance(0.86)
    if master.mode == "RGBA":  # 파생 중 알파 보존
        a = master.getchannel("A")
        hover.putalpha(a)
        pressed.putalpha(a)
    return {"Normal": normal, "Hover": hover, "Pressed": pressed}
