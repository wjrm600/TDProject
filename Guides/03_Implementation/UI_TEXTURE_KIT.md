# UI 텍스처 키트 — 매니페스트 기반 텍스처 UI 파이프라인

솔리드 컬러/절차 드로잉 UI 를 **AI 생성 텍스처**로 격상하는 파이프라인.
핵심 = **ControlNet 하이브리드**: 지오메트리(9-slice 마진, 대칭, 라운드)는 PIL 절차
도면이 잠그고, 텍스처 디테일은 AI 가 채운다.

## 확정 스타일 (2026-07-03 사용자 게이트)

**A안 — 화이트+파스텔 격상** (기존 `AOSUIStyle.h` 화이트+코랄/블루 파스텔과 연속).
마스터 프롬프트 토큰 = `ui_kit_manifest.json` 의 `style_prefix` (단일 진실 — 모든
컴포넌트 생성에 자동 결합, 일관성 보장).

## 파이프라인 (컴포넌트 1종 추가 절차)

```
1. ui_kit_manifest.json 에 컴포넌트 항목 추가
   { name, kind(panel9|button3|bar), size, final_size, prompt,
     control{draw, strength, args}, post[...], slice_margin, ue_name }
2. ComfyUI 기동 확인 (run_nvidia_gpu.bat → :8188)
3. python gen_ui_kit.py --only <name> --count 3
   → RawAssets/UI_Kit/<name>_<i>.png  (9-slice 검증 PASS/FAIL 자동 표시)
4. 후보 판독 (Claude Read or 사용자) → 선정본을 <name>_final.png 로 복사
   (button3 은 _Hover/_Pressed 도 함께 — derive_states 가 자동 생성)
5. UE 임포트: import_ui_kit.py
   - 에디터 켜짐: MCP execute_script
   - 에디터 꺼짐: UnrealEditor-Cmd.exe <proj> -ExecutePythonScript=<py> -unattended -nosplash
     (⚠️ -run=pythonscript 커맨드릿은 임포트에서 Slate 크래시)
6. 위젯 배선: AOSUIStyle::KitBrush / KitButtonStyle (아래 계약)
```

## 파일

| 파일 | 역할 |
|------|------|
| `Mcp_Tools/Asset_Pipeline/ui_kit_manifest.json` | 컴포넌트 명세 + 스타일 토큰 (단일 진실) |
| `gen_ui_kit.py` | 배치 생성 CLI (ControlNet, `--no-cn` 폴백) |
| `draw_controls.py` | PIL 절차 도면 → 제어 이미지 (panel_frame/button_plate/divider_bar/tooltip_frame) |
| `ui_postprocess.py` | symmetrize_x / mask_from_rounded_rect / validate_nine_slice / derive_states |
| `import_ui_kit.py` | [UE 에디터] 배치 임포트 (TEXTUREGROUP_UI/NO_MIPMAPS/TC_EDITOR_ICON + save) |
| `Source/.../AOSUIStyle.h` | `KitBrush`/`KitButtonStyle` (배선 헬퍼) |

## 배선 계약 (C++)

```cpp
// 9-slice 패널/이미지: 텍스처 있으면 DrawAs=Box(Margin=slice_margin), 없으면 솔리드 폴백
Border->SetBrush(AOSUIStyle::KitBrush(
    TEXT("/Game/AOS/UI/Assets/T_UI_Shop_Panel.T_UI_Shop_Panel"),
    0.18f /* = manifest slice_margin */, AOSUIStyle::CardWhite));

// 버튼 3상태 (Base / Base_Hover / Base_Pressed)
Button->SetStyle(AOSUIStyle::KitButtonStyle(
    TEXT("/Game/AOS/UI/Assets/T_UI_Shop_Button"), 0.30f, AOSUIStyle::CardWhite));
```

- **slice_margin 은 매니페스트와 배선이 같은 값을 써야 함** (임포트 로그 `CONTRACT` 줄 참고)
- **텍스트는 텍스처에 굽지 않는다** — 전부 TextBlock (로컬라이즈/선명도)
- 텍스처 미존재 상태에서도 솔리드 폴백으로 PIE 정상 (BanPick 패턴 답습)

## 원칙 / 함정 (실측)

1. **9-slice 알파에 rembg 금지** — 직선/라운드 엣지를 파먹어 slice 가 깨짐. 알파는
   절차 도면과 동일한 지오메트리(`mask_from_rounded_rect`)에서 결정적으로 유도.
2. **버튼 3상태는 3회 생성 금지** — 마스터 1장에서 `derive_states`(밝기/채도 파생).
   따로 생성하면 상태 간 디테일이 달라져 hover 시 형태가 튄다.
3. **validate_nine_slice** = 중앙 스트레치 영역 픽셀 표준편차 검사. FAIL 후보(중앙에
   장식/가짜 UI 목업이 생성된 것)는 선정에서 제외 — 실제로 후보의 절반쯤 걸러냄.
4. ControlNet 모델 = `diffusion_pytorch_model.safetensors`(SDXL scribble),
   strength 0.85. `gen_controlnet.py` 그래프 재사용 (denoise 1.0 — img2img 아님).
5. ComfyUI 위치: `C:\Users\wjrm7\Downloads\ComfyUI_windows_portable_nvidia\ComfyUI_windows_portable\run_nvidia_gpu.bat`

## 적용 현황 / 백로그

- ✅ **상점 팝업** (2026-07-03 파일럿): T_UI_Shop_Panel/Button(3)/Divider/Tooltip 임포트
  + `AOSShopWidget.cpp` 배선 (패널 9-slice 보더, 버튼 4종 스타일, 헤더 디바이더)
- ⬜ 벤픽 장식 격상: 기존 `make_banpick_*.py` PIL 도면을 `draw_controls.py` 제어
  이미지로 전환해 ControlNet 재생성 (경로/이름 유지 = C++ 무변경, 매니페스트에 추가만)
- ⬜ HP바/데미지 숫자, 라운드 결과, 메인 메뉴 순차 적용
