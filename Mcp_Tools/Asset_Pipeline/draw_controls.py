# -*- coding: utf-8 -*-
"""draw_controls.py — PIL 절차 도면 → ControlNet(scribble) 제어 이미지.

지오메트리 정밀도(9-slice 안전 마진, 대칭)는 코드가 잠그고, 텍스처 디테일은
AI 가 채우는 하이브리드의 '코드 쪽 절반'. scribble 규약 = 검은 배경 + 흰 선.

각 함수는 PIL Image (L 모드 흑백) 반환. gen_ui_kit.py 가 매니페스트의
control.draw 이름으로 호출한다.
"""
from PIL import Image, ImageDraw


def _canvas(w, h):
    img = Image.new("L", (w, h), 0)
    return img, ImageDraw.Draw(img)


def panel_frame(w, h, margin_frac=0.18, radius_frac=0.06, line=6):
    """9-slice 패널 프레임: 외곽 라운드 사각 + 마진 경계 안쪽 라인.
    장식은 마진 밴드(외곽~안쪽 라인 사이)에만 생기도록 이중 테두리로 유도."""
    img, d = _canvas(w, h)
    r = int(min(w, h) * radius_frac)
    m = int(min(w, h) * margin_frac)
    d.rounded_rectangle([line, line, w - line, h - line], radius=r,
                        outline=255, width=line)
    d.rounded_rectangle([m, m, w - m, h - m], radius=max(r - m // 2, 4),
                        outline=255, width=max(line - 2, 3))
    # 상단 중앙 장식 앵커 (문양 유도점)
    d.ellipse([w // 2 - m // 3, line * 2, w // 2 + m // 3, line * 2 + m * 2 // 3],
              outline=255, width=3)
    return img


def button_plate(w, h, radius_frac=0.28, line=5):
    """버튼 플레이트: 라운드 사각 + 내부 하이라이트 밴드 유도선."""
    img, d = _canvas(w, h)
    r = int(h * radius_frac)
    d.rounded_rectangle([line, line, w - line, h - line], radius=r,
                        outline=255, width=line)
    d.rounded_rectangle([line * 3, line * 3, w - line * 3, h // 2],
                        radius=max(r - line * 2, 3), outline=255, width=2)
    return img


def divider_bar(w, h, line=4):
    """수평 디바이더: 중앙 라인 + 중앙 마름모 장식 유도."""
    img, d = _canvas(w, h)
    cy = h // 2
    d.line([h, cy, w - h, cy], fill=255, width=line)
    s = h // 3
    d.polygon([(w // 2, cy - s), (w // 2 + s, cy), (w // 2, cy + s),
               (w // 2 - s, cy)], outline=255, width=3)
    return img


def tooltip_frame(w, h, margin_frac=0.12, radius_frac=0.10, line=4):
    """툴팁 소형 패널: 단일 라운드 테두리 (장식 최소)."""
    img, d = _canvas(w, h)
    r = int(min(w, h) * radius_frac)
    d.rounded_rectangle([line, line, w - line, h - line], radius=r,
                        outline=255, width=line)
    return img


DRAWERS = {
    "panel_frame": panel_frame,
    "button_plate": button_plate,
    "divider_bar": divider_bar,
    "tooltip_frame": tooltip_frame,
}


def draw(name, w, h, **kw):
    return DRAWERS[name](w, h, **kw)
