# 라이선스 준수 가이드 (Qt5 LGPLv3 상용 배포)

Ocean 은 Qt 상용 라이선스 없이 **Qt5 오픈소스(LGPLv3)** 로 상용 배포하는 것을 전제로 만든다.
LGPLv3 는 비용이 들지 않지만, 아래 조건을 지켜야 한다.

## 1. 코드/빌드에 이미 반영된 것

| 항목 | 반영 내용 | 위치 |
| --- | --- | --- |
| Qt 모듈 | Core, Gui, Widgets, Qml(수식용 QJSEngine), Svg, Network(백업이 직접 사용: 구글 로그인·드라이브 요청) 만 사용 (모두 LGPLv3). Qt Network Authorization 은 쓰지 않는다(§2) | `CMakeLists.txt` |
| 동적 링크 | 위 Qt 모듈(Network 포함)이 공유 라이브러리가 아니면 CMake 구성이 실패한다(확인 목록에 Network 가 있다) | `CMakeLists.txt` |
| 앱 안 고지 | 왼쪽 아래 프로필 메뉴 > "오픈소스 라이선스": `THIRD_PARTY_NOTICES.md` 전체(리소스로 넣음) + "Qt 정보" 버튼(`QMessageBox::aboutQt`). 함께 배포하는 라이브러리 · 글꼴(MIT, Apache-2.0, OFL 등)의 고지와 라이선스 전문 위치를 앱 안에서도 보인다. 필수인 까닭은 §3 의 11 | `UI/View/Shell/LicenseNoticesDialog.cpp`, `resources/Ocean.qrc` |
| 아이콘 | 외부 아이콘/에셋 없이 코드로 그린다. qmarkdowntextedit 원본의 검색창 아이콘(KDE Breeze, LGPL-3)은 지우고 앱 아이콘을 쓴다 | `UI/Common/Theme/IconPainter.cpp`, `UI/Common/Control/MarkdownSourceEdit.cpp` |
| 글꼴 | 기본 · 컴팩트 시안의 UI 는 시스템 글꼴. 번들(모두 OFL 1.1, 전문은 설치본 `licenses/`): 코드 블록·터미널용 Cascadia Mono Regular/Bold/Italic/BoldItalic, Editorial 시안 서체 Inter Regular/Medium/SemiBold · Inter Display SemiBold · IBM Plex Mono Regular/Medium(원본 그대로), Noto Serif KR Regular/SemiBold(한글 · 라틴 · 그리스 문자 · 기호만 남긴 서브셋: OFL 의 Modified Version. 라이선스와 이름 테이블에 Reserved Font Name 이 없음을 확인해 이름을 그대로 쓴다) | `Core/BundledFonts.cpp`, `resources/fonts/`, `docs/ThirdParty.md` |
| 터미널 GPU | Vulkan 렌더러용으로 Vulkan-Headers 1.4.365 (Apache-2.0 OR MIT), vk-bootstrap 1.4.365 (MIT), VulkanMemoryAllocator 3.4.0 (MIT), macOS 에서 MoltenVK 1.4.2 (Apache-2.0, 미리 만든 dylib 을 앱 번들에 동봉). `OCEAN_TERMINAL_GPU=OFF` 이면 모두 빠진다 | `external/TerminalViewer/cmake/TerminalViewerDependencies.cmake`(서브모듈), `docs/ThirdParty.md` |
| PDF | 외부 PDF 라이브러리 없이 쪽 수만 직접 센다. 본문은 앱이 읽지 않고 분석 엔진에 넘긴다(API 키: 문서 블록, Claude 구독: Claude Code 가 파일을 읽는다) | `Document/Analysis/SourceFileInfo.cpp` |
| 백업 zip 읽기·쓰기 | zip 과 CRC32 를 직접 구현한 `ZipWriter` 와, store 전용 리더를 직접 만들어 외부 zip 라이브러리·Qt private `QZipReader` 를 쓰지 않는다. `ZipWriter` 는 옛 Claude Skill 내보내기에서 쓰던 것을 2026-10-25 계획 ④ 에서 옮겼다 | `Backup/Common/ZipWriter.cpp`, `Backup/Common/ZipReader.cpp`, `Backup/Common/BackupArchive.cpp` |
| 구글 로그인 | Qt Network Authorization(GPLv3)을 쓰지 않고 `QTcpServer`(루프백 리디렉트), `QNetworkAccessManager`, `QCryptographicHash`(PKCE)로 직접 만든다. 구글 클라이언트 라이브러리도 쓰지 않는다 | `Backup/Strategy/GoogleAuthFlow.cpp` |
| 구글 드라이브 | REST v3 를 `QNetworkAccessManager` 로 직접 부른다. 스코프는 `drive.file` 하나 | `Backup/Strategy/GoogleApiClient.cpp`, `Backup/Strategy/GoogleDriveStorage.cpp` |
| 토큰 보관 | 플랫폼 API 를 직접 부른다(macOS 키체인 `Security.framework`, 윈도우 자격 증명 관리자 `advapi32`). QtKeychain 을 비롯한 새 서드파티 없음 | `Backup/Strategy/SystemTokenStore_*.cpp` |
| 구글 클라이언트 ID/비밀 | CMake 캐시 옵션(`OCEAN_GOOGLE_CLIENT_ID`, `OCEAN_GOOGLE_CLIENT_SECRET`)으로 구성할 때 받아 빌드 폴더의 생성 헤더로 넣는다. 저장소에 두지 않는다 | `cmake/GoogleClientConfig.h.in` |
| 마크다운 | md4c (MIT) 로 해석, qmarkdowntextedit (MIT) 로 원문 편집, QSourceHighlite (MIT) 로 코드 블록 구문 강조. qmarkdowntextedit 의 ghostwriter(GPL-3) 유래 코드는 지웠다 | `docs/ThirdParty.md`, `cmake/patches/` |
| 수식 | MathJax 3.2.2 (Apache-2.0) + TeX 글꼴 데이터 (OFL-1.1) 를 JS 번들로 넣는다. 빌드 스크립트가 감사한 패키지 외에는 번들에 못 들어가게 막는다 | `docs/ThirdParty.md`, `resources/mathjax/` |
| 서드파티 소스 | 저장소에 두지 않는다. 구성할 때 감사한 버전(커밋/태그 + SHA256)만 내려받고, 패치 스크립트가 원본 해시를 확인한 뒤 Ocean 수정을 적용한다 | `cmake/OceanDependencies.cmake`, `cmake/patches/`, `docs/ThirdParty.md` |
| 고지 설치 | `cmake --install` 이 고지와 전문을 macOS 는 `Ocean.app/Contents/Resources/licenses`, 그 외는 `licenses/` 에 넣는다. 이 문서(내부 지침)는 넣지 않는다 | `CMakeLists.txt` |

## 2. 새 의존성을 추가할 때

**쓰면 안 되는 Qt 모듈 (오픈소스판이 GPLv3 전용)**
Qt Charts, Qt Data Visualization, Qt Virtual Keyboard, Qt Network Authorization,
Qt Wayland Compositor, Qt Quick 3D, Qt Quick Timeline, Qt Lottie Animation.
필요하면 직접 구현하거나 상용 라이선스를 검토한다.

**서드파티 라이브러리**

| 용도 | 사용 가능 | 사용 금지 |
| --- | --- | --- |
| 수학/최적화 | Eigen (MPL2), Sophus (MIT), Ceres (BSD-3) | — |
| PDF 본문 추출 | PDFium (BSD-3 / Apache-2.0) | Poppler (GPL), MuPDF (AGPL), Ghostscript (AGPL) |
| 글꼴 번들 | Pretendard, Noto Sans KR, D2Coding, Inter, IBM Plex, Noto Serif 등 OFL. 서브셋 등 고친 글꼴은 Reserved Font Name 을 쓰지 않는다(RFN 이 있으면 이름을 바꾼다) | 라이선스가 불분명한 글꼴 |
| 마크다운/수식 | md4c (MIT), qmarkdowntextedit (MIT, Ocean 패치본), QSourceHighlite (MIT), MathJax v3 + TeX 글꼴 (Apache-2.0 + OFL) | MicroTeX (JLaTeXMath(GPL-2.0+) 파생으로 보임), MathJax v4 기본 글꼴 mathjax-newcm (GUST/LPPL, 원본 New Computer Modern 8.0 부터 GPL) |
| 문서 도킹 | Qt Advanced Docking System (LGPL-2.1, 공유 라이브러리로만. UXWing 핀 아이콘 4개는 빼야 함) | KDDockWidgets (GPL/상용) |
| 터미널 렌더러 | Vulkan-Headers, vk-bootstrap (MIT), VulkanMemoryAllocator (MIT), MoltenVK (Apache-2.0) | kitty 코드(GPL-3, 박스 문자 등), QTermWidget·Konsole (GPL) |

> 개발 PC 에 Homebrew 로 `poppler` 가 설치되어 있어도 링크하지 않는다.
> 새 라이브러리를 넣으면 `THIRD_PARTY_NOTICES.md` 에 이름, 버전, 라이선스를 추가한다.

**외부 라이브러리/에셋 도입 절차 (파일 단위 감사)**

1. 최상위 LICENSE 와 **모든 파일 헤더**가 같은 라이선스인지 확인한다 (SPDX, GPL, "General Public", "taken from", "based on", URL 검색).
2. **바이너리에 들어가는 에셋**(.qrc 로 넣는 아이콘, 글꼴, 데이터)의 출처와 라이선스를 하나씩 확인한다. 출처를 모르면 넣지 않는다.
   글꼴은 공식 배포본에서 받아 압축 · 파일 SHA256 을 남기고, 라이선스 파일의 저작권 줄과 RFN, 이름 테이블의 저작권 · 상표 · 라이선스 레코드를 확인한다(예: `docs/ThirdParty.md` 의 Editorial 시안 서체 절).
3. 다른 프로젝트에서 옮겨 온 코드(GPL 프로그램, StackOverflow 의 CC BY-SA 등)를 찾는다. 필요하면 상류 저장소와 원문을 비교한다.
4. 플랫폼별로만 컴파일되는 파일, 전이 의존성(빌드가 끌어오는 패키지)도 확인한다.
5. 지켜야 할 의무(고지, 전문 동봉, 소스 제공, 동적 링크, EULA 조항)를 정리하고, 결과와 Ocean 수정 내역을 `docs/ThirdParty.md` 에 남기고, `cmake/OceanDependencies.cmake` 에 URL 과 SHA256 을 고정한다(수정이 있으면 `cmake/patches/<이름>.cmake` 에 패치 전/후 해시와 함께). 터미널 렌더러의 의존성은 TerminalViewer 저장소의 `cmake/TerminalViewerDependencies.cmake` 에 고정한다(Ocean 에서는 `external/TerminalViewer/`).

## 3. 배포 체크리스트

1. **Qt 를 동적 라이브러리로 함께 배포한다.** 사용자가 Qt 라이브러리를 다른 버전으로 바꿔 끼울 수 있어야 한다.
   - Windows: `windeployqt Ocean.exe` → Qt DLL 이 실행 파일 옆에 놓인다.
   - macOS: `macdeployqt Ocean.app` → `Ocean.app/Contents/Frameworks` 에 놓인다.
   - 단일 실행 파일로 묶는 패커, 정적 링크는 쓰지 않는다.
2. **라이선스 전문을 동봉한다.** 구성할 때 `-DOCEAN_QT_LICENSE_DIR=<Qt 설치 경로>/share/qt/3rd_party_licenses` 를 주면
   `cmake --install` 이 `LICENSE.LGPLv3`, `LICENSE.GPLv3`, Qt 안에 포함된 서드파티 고지(`licenses/qt-third-party`)를 함께 설치한다.
   이 값이 없으면 라이선스 전문이 빠지므로 배포 패키지를 만들 때는 반드시 지정한다.
3. **Qt 소스를 제공한다.** 배포한 Qt 바이너리와 정확히 같은 버전의 Qt 소스를 함께 주거나, 받을 수 있는 경로를 고지한다.
   빌드에 쓴 Qt 소스 아카이브를 사내에 보관해 둔다.
4. **Qt 소스를 수정하지 않는다.** 수정했다면 수정한 Qt 소스도 같은 방식으로 공개해야 한다.
5. **EULA 를 확인한다.** EULA 에 리버스 엔지니어링 금지 조항이 있더라도, Qt 부분을 수정하고 그 수정을 디버깅하기 위한
   리버스 엔지니어링은 허용해야 한다. LGPL-2.1 라이브러리를 넣으면 범위가 더 넓어진다
   (§6: 결합된 저작물 전체에 대해 "고객 자신의 사용을 위한 수정과 그 디버깅을 위한 리버스 엔지니어링"을 허용).
   예시 조항: "본 소프트웨어에 포함된 LGPL 라이브러리와 관련하여, 본 계약의 어떤 조항도 귀하가 자신의 사용을 위해
   본 소프트웨어를 수정하거나 그 수정을 디버깅하기 위해 리버스 엔지니어링하는 것을 제한하지 않는다."
6. **Qt 상표를 제품명에 쓰지 않는다.** 앱 이름/아이콘에 "Qt" 를 넣지 않는다.
7. 앱을 하드웨어 기기에 탑재해 판매하는 경우(LGPLv3 의 "User Product") 설치 정보 제공 의무가 추가되므로 별도로 검토한다.
8. **배포 패키지는 `-DOCEAN_RELEASE_PACKAGE=ON` 으로 구성한다.** Qt 라이선스 경로가 없으면 구성이 실패한다.
9. **배포 도구가 넣는 Qt 플러그인을 확인한다.** `macdeployqt`/`windeployqt` 는 설치된 플러그인을 통째로 넣을 수 있다
   (Anaconda Qt 에는 qpdf, qtiff, qwebp 등이 있다). 쓰지 않는 플러그인은 빼고, 넣은 모듈은 고지에 반영한다.
10. **배포 빌드에 Anaconda Qt 를 쓰지 않는다.** ICU, libpng, zstd 등이 함께 들어가 고지가 늘어난다.
11. **앱 안 고지 화면을 유지한다(필수).** 프로필 메뉴 > "오픈소스 라이선스" 가 `THIRD_PARTY_NOTICES.md` 를 보여 준다.
    지금도 필수다: Qt 의 LGPLv3 §4(c) 는 실행 중 저작권 고지를 보이는 앱이면 그 사이에 Qt 의 저작권 고지와 GPL · LGPL 전문 위치를 넣도록 요구한다("Qt 정보" 버튼과 고지 글이 그 자리다).
    LGPL-2.1 라이브러리를 넣으면 그 §6 도 같은 것을 요구한다.
    새 라이브러리를 넣으면 `THIRD_PARTY_NOTICES.md` 만 고치면 화면에도 반영된다.
12. **윈도우 HTTPS(TLS)를 정한다.** 아래 §5 의 선택지 중 하나를 골라 배포본에 반영하고, 고른 방법의 라이선스 고지(OpenSSL/SSLeay 등)를 `THIRD_PARTY_NOTICES.md` 에 더한다. 정하지 않은 채 윈도우를 배포하면 구글 드라이브 백업이 TLS 오류로 실패할 것으로 본다(확인하지 않았다: [스펙 §10](superpowers/specs/2026-10-14-cloud-backup-design.md) 손 확인 목록). macOS 는 실행 중 `QSslSocket::sslLibraryVersionString()` 으로 TLS 백엔드를 확인한다.

## 4. 빌드에 쓰는 Qt

- 개발 PC 의 Qt 5.15.2 는 Anaconda 에 포함된 오픈소스판이다.
- 배포 빌드는 Qt 공식 오픈소스 배포본(5.15, LGPLv3)으로 만들고, 사용한 정확한 버전을 `THIRD_PARTY_NOTICES.md` 에 적는다.
- Windows MinGW 빌드(Qt `mingw81_64`)는 빌드 폴더와 `cmake --install` 결과에 MinGW 런타임 DLL 이 함께 들어간다: `libstdc++-6.dll`, `libgcc_s_seh-1.dll`(GPLv3 + GCC Runtime Library Exception), `libwinpthread-1.dll`(MIT 계열). MinGW 빌드는 개발용으로만 쓰고 배포하지 않는다: MinGW 로 `-DOCEAN_RELEASE_PACKAGE=ON` 을 주면 구성이 실패한다(배포 패키지는 MSVC 로 만든다).

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="<Qt5 설치 경로>"
cmake --build build
cmake --install build --prefix dist
```

## 5. 알려진 공백: 윈도우 HTTPS(TLS)

구글 로그인·드라이브 요청은 HTTPS 다. Qt 5.15 의 TLS 는 플랫폼마다 다르다.

- **공식 Qt 5.15.2 윈도우**: OpenSSL **1.1.1** DLL(`libssl-1_1-x64.dll`, `libcrypto-1_1-x64.dll`)이 실행 파일 옆이나 PATH 에 있어야 HTTPS 가 된다. 없으면 구글 연결·백업이 TLS 오류로 실패할 것으로 본다(폴더 백업은 영향 없음). DLL 이 없을 때 실제로 어떤 오류가 보이는지는 이 PC 에서 확인하지 않았다: [스펙 §10](superpowers/specs/2026-10-14-cloud-backup-design.md) 손 확인 목록.
- OpenSSL 1.1.1 은 2023-09-11 에 지원이 끝났고, OpenSSL/SSLeay 라이선스(광고 문구 조항)라 DLL 을 동봉하면 고지가 늘어난다.
- **공식 Qt 5.15.2 macOS**: SecureTransport 가 기본이라 OpenSSL 이 필요 없을 것으로 본다. 실제로 확인하지 않았다(`QSslSocket::sslLibraryVersionString()`).
- **배포 전에 조사 문서([§5.2](superpowers/research/2026-10-13-cloud-backup-research.md))의 네 선택지 중 하나로 정한다**: ① 1.1.1 DLL 동봉(지원 종료) ② Qt 5.15.2 를 OpenSSL 3.5 LTS(Apache-2.0, 2030-04-08 까지 지원)에 `-openssl-linked` 로 직접 빌드 ③ OpenSSL 3 를 읽는 5.15.8 이상 `-lts-lgpl` 소스를 빌드 ④ `-schannel` 로 빌드해 OpenSSL 을 아예 빼기. 정하기 전에는 윈도우 배포본에서 구글 드라이브 백업이 된다고 알리지 않는다.
- 테스트는 이 공백과 상관없이 돈다(가짜 서버가 `http://127.0.0.1` 이다).
