# 트러블슈팅 — UE 에디터 + Claude Desktop 동시 실행 시 강제 재부팅 (BSOD 0x50)

**작성일**: 2026-05-28
**환경**: Windows 11 (빌드 26100 / 24H2), UE 5.7, Claude Desktop + MCP(`ue_rag_mcp.py`)
**분류**: 호스트 개발환경 트러블슈팅 (게임 코드 무관)

---

## 한 줄 요약

UE 에디터를 켠 뒤 Claude Desktop을 실행하면 PC가 강제 재부팅되던 문제 →
**오래전 삭제된 한국 DRM 프로그램 `MarkAny ePageSafer`가 남긴 고아 커널 드라이버 `cbfltfs4.sys`(2017년 EldoS CBFS 필터)** 가 원인.
이 드라이버를 **비활성화/제거**하면 해결. **RAM·전원 문제 아님.**

---

## 증상

- UE 에디터 실행 → Claude Desktop 실행하면 수 초~수 분 내 **블루스크린 없이(혹은 순간 BSOD 후) 즉시 재부팅**.
- 3일간(5/25~5/28) **5회** 발생.

---

## 진단 과정

### 1. 이벤트 로그 — 크래시 종류 확인

`Get-WinEvent`로 System 로그 조회:

- **Kernel-Power 41** 다수 (비정상 종료)
- **BugCheck 1001** — `0x00000050` **반복** → BSOD 확정, 재현되는 단일 원인 존재
- **WHEA-Logger 없음** → CPU/전원/메모리의 *전기적* 하드웨어 사망은 아님
- 디스크 SMART 전부 `Healthy`

> `0x50 = PAGE_FAULT_IN_NONPAGED_AREA` — 해제됐거나 잘못된 커널 메모리를 참조. 주원인: RAM 불량 / 드라이버 버그 / 시스템파일 손상.

미니덤프 위치: `C:\Windows\Minidump\*.dmp` (관리자 권한으로만 읽힘).

### 2. WinDbg `!analyze -v` — 범인 모듈 특정

- `winget install Microsoft.WinDbg` 로 설치 → `cdb.exe`(콘솔 디버거) 사용
- `C:\Windows\Minidump` 는 관리자 전용이라, 관리자 PowerShell에서 덤프를 일반 폴더로 복사 후 분석

```
cdb -z <dump> -y srv*C:\Symbols*https://msdl.microsoft.com/download/symbols -c "!analyze -v; q"
```

**핵심 결과 (최신 덤프 052826):**

```
PAGE_FAULT_IN_NONPAGED_AREA (50)
IMAGE_NAME:  cbfltfs4.sys
MODULE_NAME: cbfltfs4
PROCESS_NAME: python.exe                 ← Claude Desktop이 띄우는 unreal-rag MCP 서버
FAILURE_BUCKET_ID: AV_cbfltfs4!unknown_function
*** WARNING: Unable to verify timestamp for cbfltfs4.sys

스택:
  nt!KeBugCheckEx
  nt!MiSystemFault / MmAccessFault / KiPageFault
  Ntfs!NtfsInitializeIrpContextInternal
  nt!IofCallDriver
  cbfltfs4+0x26180                        ← 필터 드라이버가 NTFS 위 I/O 스택에서 폴트
```

- **교차 확인**: 다른 날짜 덤프(052526)도 **완전히 동일** (`0x50` / `cbfltfs4.sys` / `python.exe` / 같은 버킷) → 우연 아님.

### 3. 드라이버 소유 앱 식별

| 항목 | 값 |
|---|---|
| 파일 | `C:\Windows\System32\drivers\cbfltfs4.sys` |
| 버전 | 4.1.105.114 (**2017-05-25**) |
| 제품 | CallbackFiler — "/n software, Inc." (구 EldoS CBFS Filter) |
| 서명자 | `CN=EldoS Corporation` (2010 VeriSign CA, 구 인증서) |
| 서비스 | `cbfltfs4`, Start=0(**Boot**), Type=2(파일시스템 드라이버), Group=Filter |
| 설치 흔적 | `C:\Program Files (x86)\markany\ePageSafer\drivers\cbfltfs4.sys` |
| 설치 목록/언인스톨러/프로세스 | **없음** |

→ **MarkAny ePageSafer**(은행·관공서·전자문서·전자책 사이트가 자동 설치하는 한국 DRM/보안 플러그인)가 깐 드라이버.
**앱 본체는 이미 삭제됐는데 커널 필터만 고아로 남아 Boot 시작으로 상시 로드**되는 상태.

---

## 근본 원인

1. 2017년산 구버전 파일시스템 필터 드라이버 `cbfltfs4.sys`가 최신 Windows 11(24H2)에서 비호환.
2. 매 부팅 시 로드되어 NTFS 위 I/O 스택에 상주.
3. UE 에디터 + Claude Desktop(`python.exe` MCP 서버)이 **파일 I/O를 폭증**시키면 이 필터를 자주 통과 → 해제된 메모리 참조 → `0x50` BSOD → 강제 재부팅.

즉 "에디터 + Claude Desktop 동시 실행"은 트리거(부하)일 뿐, 원인은 고아 DRM 드라이버.

---

## 해결 방법 (관리자 PowerShell 필요 — 시스템 드라이버 변경)

> `sc.exe`로 실행할 것 (PowerShell의 `sc`는 `Set-Content` 별칭). `start=` 뒤 **공백** 필수.

### 1단계 — 비활성화 후 재부팅 (안전·되돌리기 쉬움, 권장)

```powershell
sc.exe config cbfltfs4 start= disabled
# 재부팅
```

### 2단계 — 며칠 안정 확인 후 완전 제거 (선택)

```powershell
sc.exe stop cbfltfs4
sc.exe delete cbfltfs4
Remove-Item "C:\Program Files (x86)\markany" -Recurse -Force -ErrorAction SilentlyContinue
Remove-Item "C:\Windows\System32\drivers\cbfltfs4.sys" -Force -ErrorAction SilentlyContinue
```

> System32의 `.sys`가 잠겨 안 지워지면 1단계 비활성화 → 재부팅 후 삭제.

### 롤백 (문제 시)

```powershell
sc.exe config cbfltfs4 start= boot
```

---

## 검증

재부팅 후 **UE 에디터 + Claude Desktop 동시 실행**으로 부하 재현:

- `C:\Windows\Minidump` 에 **새 덤프 미생성**
- 이벤트 로그에 **Kernel-Power 41 / BugCheck 1001 미발생**

확인 명령:

```powershell
Get-WinEvent -FilterHashtable @{LogName='System'; Id=1001} -MaxEvents 3 |
  Select-Object TimeCreated, Id
Get-ChildItem C:\Windows\Minidump\*.dmp | Sort LastWriteTime -Desc |
  Select-Object Name, LastWriteTime -First 3
```

---

## 재발 방지 / 참고

- **ePageSafer는 일부 한국 사이트가 자동 재설치**할 수 있음. 재설치되면 최신 버전이 깔리므로, 핵심은 이 2017년 잔재 드라이버 제거.
- 향후 BSOD/강제 재부팅 시 동일 절차로 진단:
  1. 이벤트 로그(Kernel-Power 41 / BugCheck 1001 / WHEA) 확인
  2. `cdb -z <dump> -c "!analyze -v; q"` 로 `IMAGE_NAME` / `FAILURE_BUCKET_ID` 확인
  3. 드라이버 파일 버전·서명·설치 경로로 소유 앱 추적
- 이 문제는 **게임 코드(TDProject)와 무관** — 진단 중 작업하던 "스킬 상하체 분리 Part A(C++)"는 별개로 완료 상태.

---

## 부록 — 진단에 사용한 명령 모음

```powershell
# 1) 셧다운/BSOD 원인
Get-WinEvent -FilterHashtable @{LogName='System'; Id=41}   -MaxEvents 5
Get-WinEvent -FilterHashtable @{LogName='System'; Id=1001} -MaxEvents 5
Get-WinEvent -FilterHashtable @{LogName='System'; ProviderName='Microsoft-Windows-WHEA-Logger'} -MaxEvents 5

# 2) 디버거 설치 + 덤프 분석 (덤프는 관리자 권한 폴더에서 복사 후)
winget install -e --id Microsoft.WinDbg
$cdb="C:\Program Files\WindowsApps\Microsoft.WinDbg_*_x64__8wekyb3d8bbwe\amd64\cdb.exe"
& (Get-Item $cdb) -z <dump> -y "srv*C:\Symbols*https://msdl.microsoft.com/download/symbols" -c "!analyze -v; q"

# 3) 드라이버 식별
Get-Item C:\Windows\System32\drivers\cbfltfs4.sys | Format-List VersionInfo
Get-AuthenticodeSignature C:\Windows\System32\drivers\cbfltfs4.sys
Get-ItemProperty "HKLM:\SYSTEM\CurrentControlSet\Services\cbfltfs4"
```
