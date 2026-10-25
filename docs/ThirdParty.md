# 서드파티 라이브러리와 에셋

Ocean 저장소에는 서드파티 **소스 코드**를 두지 않는다. CMake 구성 단계가 감사한 버전을 정확히 내려받고(`cmake/OceanDependencies.cmake`,
터미널 GPU 렌더러 의존성은 서브모듈의 `external/TerminalViewer/cmake/TerminalViewerDependencies.cmake`),
라이선스 감사 결과에 따른 Ocean 수정을 적용한다(`cmake/patches/`). 저장소에 남긴 서드파티 파일은 MathJax 번들(생성물)과 글꼴(Cascadia Mono, Editorial 시안 서체)뿐이다.

- 상용 배포 라이선스 정책과 감사 절차: `docs/LicenseCompliance.md`
- 앱 안 고지: `THIRD_PARTY_NOTICES.md`
- 패치 파일 주석의 "OCEAN_README.md" 는 이 문서의 해당 라이브러리 절을 말한다.

| 이름 | 얻는 곳 | 고정 | 라이선스 | Ocean 수정 |
| --- | --- | --- | --- | --- |
| md4c | 내려받기 | 커밋 `c7ba975c34d714966ea910c58a97524e5d674ff7` | MIT | 없음 |
| qmarkdowntextedit | 내려받기 | 커밋 `11e7a3530d4b6e28eee90cd4292c35be4bdd4112` | MIT | `cmake/patches/qmarkdowntextedit.cmake` |
| QSourceHighlite | 내려받기 | 커밋 `d78f0d4171580a18646b15425c449db3f2bb240a` | MIT | `cmake/patches/QSourceHighlite.cmake` |
| Vulkan-Headers (`OCEAN_TERMINAL_GPU`) | 내려받기 | 태그 `v1.4.365` | Apache-2.0 OR MIT | 없음 |
| vk-bootstrap (`OCEAN_TERMINAL_GPU`) | 내려받기 | 태그 `v1.4.365` | MIT | 없음 |
| VulkanMemoryAllocator (`OCEAN_TERMINAL_GPU`) | 내려받기 | 태그 `v3.4.0` | MIT | 없음 |
| MoltenVK (`OCEAN_TERMINAL_GPU`, macOS) | 내려받기(미리 만든 dylib) | 릴리스 `v1.4.2` `MoltenVK-macos.tar` | Apache-2.0 (+ 정적으로 들어간 코드, 아래) | 없음 |
| MathJax 번들 | 저장소 `resources/mathjax/` | mathjax-full 3.2.2, mhchemparser 4.2.1 | Apache-2.0, 글꼴 데이터 OFL-1.1 | 진입 코드·빌드 스크립트는 Ocean 작성 |
| Cascadia Mono | 저장소 `resources/fonts/` | 2407.24 (Regular, Bold, Italic, BoldItalic, 글꼴 버전 2407.024) | OFL-1.1 | 없음 |
| Inter | 저장소 `resources/fonts/` | 릴리스 `v4.1` (Regular, Medium, SemiBold, Display SemiBold, 글꼴 버전 4.001) | OFL-1.1 | 없음 |
| IBM Plex Mono | 저장소 `resources/fonts/` | 릴리스 `@ibm/plex-mono@2.5.0` (Regular, Medium, 글꼴 버전 2.005) | OFL-1.1 (RFN "Plex") | 없음 |
| Noto Serif KR | 저장소 `resources/fonts/` | 릴리스 `Serif2.003` (Regular, SemiBold, 글꼴 버전 2.003) | OFL-1.1 | 서브셋(한자 등을 뺐다, 아래 절) |

## 내려받기와 패치

- 각 라이브러리는 GitHub/GitLab 소스 압축(`archive/<커밋|태그>.tar.gz`)을 `URL_HASH SHA256` 으로 고정해 내려받는다. 해시가 다르면 구성이 멈춘다.
- 패치 스크립트는 고칠 파일의 SHA256 이 감사한 원본과 같을 때만 고치고, 고친 결과의 SHA256 도 확인한다. 이미 고친 파일(결과 해시와 같음)은 건너뛴다.
- 패치는 앵커 문자열 사이를 바꾸거나 지우는 방식이다. 유니파이드 diff 는 쓰지 않는다(지운 GPL 코드가 diff 의 삭제 줄로 저장소에 남기 때문).
- 패치 스크립트(`cmake/patches/*.cmake`)는 줄바꿈까지 해시로 확인하므로 `.gitattributes` 로 LF 를 고정한다.
- 패치 결과는 모든 플랫폼에서 바이트 그대로, 줄바꿈 LF 로 쓴다(`file(WRITE)` 대신 바이너리 쓰기: `ocean_patch_write_bytes`). Windows 에서도 결과 해시가 같다.
- 해시가 다르다는 오류는 감사한 해시를 고쳐서 없애지 않는다. 안내대로 `build/_deps/<이름>-*` 를 지우고 다시 구성한다. 해시는 새 버전의 라이선스를 다시 감사한 뒤에만 바꾼다(아래 "버전을 올릴 때").
- 처음 구성 뒤에는 `-DFETCHCONTENT_FULLY_DISCONNECTED=ON` 으로 인터넷 없이 구성할 수 있다.
- 패치 스크립트를 고쳤으면 `build/_deps/<이름>-*` 를 지우고 다시 구성한다(URL 이 같으면 다시 내려받지 않아 패치도 다시 돌지 않는다).
- 구성할 때마다 qmarkdowntextedit, QSourceHighlite 소스가 패치된 상태인지 다시 확인한다(패치 스크립트를 `-DOCEAN_PATCH_VERIFY=ON` 으로 돌려 고치지 않고 확인만 한다). 아니면 구성이 멈춘다.
  - 오래되었거나 일부만 패치된 `build/_deps` 폴더(패치 스크립트를 고쳤거나 `FETCHCONTENT_FULLY_DISCONNECTED=ON` 으로 예전 소스가 남은 경우): "sources are not in the patched state … delete build/_deps/<이름>-* and reconfigure". 그 폴더를 지우고 `FETCHCONTENT_FULLY_DISCONNECTED=OFF` 로 다시 구성한다.
  - 패치하지 않은 `FETCHCONTENT_SOURCE_DIR_<NAME>` 지정: "the source override is not patched". 지정을 비우거나, 그 폴더에 먼저 패치 스크립트를 돌린다(`cmake -DSOURCE_DIR=<폴더> -P cmake/patches/<이름>.cmake`).
  - 패치 스크립트를 고치면 다음 빌드가 자동으로 다시 구성해 이 확인을 돌린다.
- `-DOCEAN_RELEASE_PACKAGE=ON` 이면 `FETCHCONTENT_SOURCE_DIR_QMARKDOWNTEXTEDIT`, `FETCHCONTENT_SOURCE_DIR_QSOURCEHIGHLITE` 지정을 거부한다. 배포 패키지는 Ocean 이 내려받아 패치한 감사 소스로만 만든다.

### 버전을 올릴 때

1. `docs/LicenseCompliance.md` §2 의 감사 절차를 새 버전에 다시 밟는다.
2. `cmake/OceanDependencies.cmake` 의 URL 과 SHA256 을 바꾼다(터미널 GPU 렌더러 의존성은 TerminalViewer 저장소의 `cmake/TerminalViewerDependencies.cmake` 를 바꾸고 서브모듈을 올린다).
3. 패치가 있는 라이브러리는 패치 스크립트의 원본 해시, 앵커, 바꿔 넣을 내용, 결과 해시를 새 원본에 맞춰 고친다.
4. 이 문서의 표와 해당 절을 고친다.

## md4c

- 원본: https://github.com/mity/md4c (커밋 `c7ba975c34d714966ea910c58a97524e5d674ff7`)
- 라이선스: MIT (`LICENSE.md`), (c) 2016-2026 Martin Mitáš
- 쓰는 파일: `src/md4c.c`, `src/md4c.h` (정적 라이브러리)
- 수정: 없음

## qmarkdowntextedit

- 원본: https://github.com/pbek/qmarkdowntextedit (커밋 `11e7a3530d4b6e28eee90cd4292c35be4bdd4112`)
- 라이선스: MIT (`LICENSE`). 저작권: (c) 2014-2026 Patrizio Bekerle, `markdownhighlighter.*`·`qownlanguagedata.*` 일부 (c) 2019-2021 Waqar Ahmed
- 쓰는 파일: `qmarkdowntextedit.*`, `markdownhighlighter.*`, `qownlanguagedata.*`, `qplaintexteditsearchwidget.*`, `linenumberarea.h` (정적 라이브러리)

### Ocean 수정 (2026-10-04, 라이선스 감사 결과) — `cmake/patches/qmarkdowntextedit.cmake`

1. `qmarkdowntextedit.cpp` `QMarkdownTextEdit::paintEvent` — 코드 블록 뒤에 둥근 사각형을 그리던 부분을 지웠다(`OCEAN PATCH` 주석).
   원본 주석에 "Code taken from ghostwriter" 라고 되어 있고, ghostwriter 는 **GPL-3.0-or-later** 다.
   남긴 기능(오른쪽→왼쪽 글자 정렬 수정, 현재 줄 강조, 목록 줄바꿈 들여쓰기)은 Ocean 이 다시 작성했다.
   코드 블록 배경은 구문 강조 서식(배경색)으로 계속 보인다. 패치 스크립트에는 Ocean 이 다시 쓴 내용만 들어 있다.
2. 검색창 아이콘 — `media/` 폴더와 `media.qrc` 를 빌드에 넣지 않고, `qplaintexteditsearchwidget.ui` 의 아이콘 지정 6개와 리소스 참조를 지운다.
   아이콘 6개가 KDE Breeze 아이콘(**LGPL-3.0-or-later**)과 같은 파일이었다.
   대신 `src/UI/Common/Control/MarkdownSourceEdit.cpp` 가 앱이 그린 아이콘(`IconPainter`)을 버튼에 넣는다.

## QSourceHighlite

- 원본: https://github.com/Waqar144/QSourceHighlite (커밋 `d78f0d4171580a18646b15425c449db3f2bb240a`)
- 라이선스: MIT (`LICENSE`)
- 쓰는 파일: `qsourcehighliter.*`, `qsourcehighliterthemes.*`, `languagedata.*` (정적 라이브러리). 데모 앱, 테스트 파일, qmake 파일은 쓰지 않는다.

### Ocean 수정 — `cmake/patches/QSourceHighlite.cmake`

1. `qsourcehighliter.h` — `setTokenFormat(Token, QTextCharFormat)` 한 줄을 더한다(`OCEAN PATCH` 주석).
   원본은 내장 테마(Monokai)만 고를 수 있어서 앱의 Visual Studio 라이트/다크 색을 넣을 방법이 없다.
   코드에서는 `src/UI/Common/Control/CodeHighlighter.cpp` 가 테마 색을 넣는다.

## 뺀 라이브러리 (2026-10-25)

- Qt Advanced Docking System 5.1.1 (LGPL-2.1) · Eigen 5.0.1 (MPL-2.0): 옛 문서 탭 · 분할 화면과 옛 규칙 맞춤(RuleMatcher)만 썼다. 학습 메뉴 계획 ④(C1 · C2)에서 뺐다.
  감사 · 패치 기록(이 문서의 옛 절, `cmake/patches/QtAds.cmake`, `cmake/PackageQtAdsSource.cmake`)은 git 기록(태그 `legacy-screens-final`, 커밋 `f89967b`, 원격에도 있음)에 있다. 다시 넣으면 새로 감사한다.

## 터미널 GPU 렌더러 의존성 (2026-10 감사, `OCEAN_TERMINAL_GPU=ON`)

서브모듈 TerminalViewer 의 `external/TerminalViewer/cmake/TerminalViewerDependencies.cmake` 가 내려받는다(Ocean 이 `OCEAN_TERMINAL_GPU` 를 `TERMINALVIEWER_GPU` 로 넘긴다. MoltenVK 는 `if(APPLE)`). 서드파티 CMake 는 돌리지 않고 대상은 TerminalViewer 가 만든다.
라이선스 전문은 Ocean `CMakeLists.txt` 가 `terminalviewer_install_licenses()` 로 아래 표의 이름 그대로 설치한다. `OFF` 이면 하나도 내려받지 않는다. 기본값은 macOS `ON`, Windows `OFF` 다(Windows 에서 GPU 렌더러는 Windows 빌드 확인 후 켠다).

| 이름 | 압축 SHA256 | 쓰는 파일 | 라이선스 | 의무 (동봉할 전문 파일) |
| --- | --- | --- | --- | --- |
| Vulkan-Headers v1.4.365 | `ed832b3292cce324ebba6522fd2d3004bf1b797fa49bfbcad0b302ff23d56abc` | `include/vulkan/`, `include/vk_video/` (모든 파일에 `SPDX-License-Identifier: Apache-2.0 OR MIT`) | Apache-2.0 OR MIT | 저작권(Khronos) 고지 + 전문 → `LICENSE.Vulkan-Headers` (Apache-2.0 전문). NOTICE 파일 없음 |
| vk-bootstrap v1.4.365 | `35eef8e046ca0a63f2ffc180a52561081d71bd18d19395a45fb78eb095423b4b` | `src/VkBootstrap*.{h,cpp,inl}` | MIT | 저작권 + 허가 문구 → `LICENSE.vk-bootstrap-MIT` |
| VulkanMemoryAllocator v3.4.0 | `822aa850c6ce77346ae96a8a1d351d52e77e85929f35363849a0a4e638e0a2a1` | `include/vk_mem_alloc.h` | MIT | 저작권(AMD) + 허가 문구 → `LICENSE.VulkanMemoryAllocator-MIT` |
| MoltenVK v1.4.2 `MoltenVK-macos.tar` | `f95765a6229cb7b915990a2890ce12ebe36a730b021545d3d52ae69ce4c4024e` | `MoltenVK/dynamic/dylib/macOS/libMoltenVK.dylib` (SHA256 `aef00b13bcc808adf15b85bef9ae67393d92be7ed5dfe41cad16fa809e4a4c5f`) | Apache-2.0 | 전문 → `LICENSE.MoltenVK-Apache-2.0`, 정적으로 들어간 코드의 고지 → `LICENSE.MoltenVK-embedded.txt` |
| Cascadia Mono Bold/Italic/BoldItalic | zip `e67a68ee3386db63f48b9054bd196ea752bc6a4ebb4df35adce6733da50c8474` | `ttf/static/CascadiaMono-{Bold,Italic,BoldItalic}.ttf` | OFL-1.1 | 기존 Regular 와 같다(`LICENSE.CascadiaMono-OFL`) |

- Vulkan-Headers: 파일 50개(`include/`)가 모두 SPDX 줄을 가진다. GPL/LGPL 문구 없음. `LICENSE.md` 가 말하는 예외(`parse_dependency.py`)는 `registry/` 의 스크립트라 쓰지 않는다.
- vk-bootstrap: 컴파일하는 파일은 `src/` 의 5개뿐이고 모두 MIT 머리말이다(`VkBootstrapDispatch.h` 는 Cody Goodson 저작권 줄이 추가). 생성 코드(`VkBootstrapDispatch.h`, `*FeatureChain*`)는 Vulkan 레지스트리에서 만든 자체 코드로 외부 코드 인용 문구가 없다. GPL/"taken from" 검색 결과 없음.
- VMA: 헤더 한 개, MIT. GPL 문구 없음.
- MoltenVK: 미리 만든 dylib 을 그대로 앱 번들 `Contents/Frameworks/libMoltenVK.dylib` 에 넣는다. 동적으로 연결한다(앱이 `dlopen`/로더로 불러옴). v1.4.2 `ExternalRevisions/` 기준 정적으로 들어간 코드와 고지 의무:
  - SPIRV-Cross `6c09849` (Apache-2.0 OR MIT, Arm), SPIRV-Tools `0d6fd73` (Apache-2.0, NOTICE 파일 없음), SPIRV-Headers `29981f6` (Khronos MIT 계열), Vulkan-Headers `e3b1eec` (Apache-2.0 OR MIT), cereal `a56bad8` (BSD-3-Clause: 바이너리 배포에 저작권·조건·면책 문구를 문서로 동봉).
  - volk, Vulkan-Tools 는 MoltenVK 의 Cube 데모 전용이라 dylib 에 들어가지 않는다(`ExternalRevisions/README.md`). glslang 은 목록에 없고 dylib 의 문자열에도 SPIR-V 생성기 이름뿐이다.
  - dylib 의 `strings` 에 GPL/GNU General 문구는 없다. 확인한 것: LICENSE(Apache-2.0), 위 저장소들의 LICENSE, 동적 의존성(시스템 프레임워크뿐). 확인하지 못한 것: 바이너리 안의 개별 오브젝트 단위 구성(소스 목록과 문자열로만 대조), MoltenVK 가 내부에서 쓰는 다른 헤더 인용.
  - 서명: 릴리스의 dylib 은 ad-hoc 서명이다. 앱 번들에 넣은 뒤 `codesign --force --deep -s - Ocean.app` 또는 배포 서명으로 다시 서명한다.
- Cascadia Mono 세 파일: 글꼴 이름 테이블(저작권 `© 2021 Microsoft Corporation`, 라이선스 설명 nameID 13, URL nameID 14 `https://scripts.sil.org/OFL`)이 기존 Regular 와 글자까지 같고, 글꼴 버전은 모두 `2407.024` 다. 파일 SHA256: Bold `b22cb603ed23cac36e8444846e1841caca21719a5e852780a8d37ae7a49b0a36`, Italic `8d98a33e7617c14dd1d200b4900bc6c2d5d7165abb5c09e9d2fd51c38ce9c9ff`, BoldItalic `ecaeab55e6334408f8ff1fde78a99ddaeb3bb7fad4fcd177df8475c88a7d240f`, Regular(기존) `06520d032ec274fa5040b22c6f4a1d829081b24ba40b2da56dae89bf10c7b481`.

## MathJax (저장소에 남긴 번들)

미리보기의 LaTeX 수식(`$...$`, `$$...$$`)을 그린다. MathJax 를 Qt 의 자바스크립트 엔진(QJSEngine, Qt Qml)에서 실행해
TeX 를 SVG 로 바꾸고, Qt Svg 로 이미지를 만든다(`src/UI/Common/Control/MathRenderer.cpp`).

| 파일 (`resources/mathjax/`) | 내용 |
| --- | --- |
| `mathjax-bundle.js` | 생성물. `entry.js` + 아래 패키지를 esbuild 로 묶은 것. 앱 리소스(`mathjax.qrc`)로 들어간다 |
| `entry.js`, `build.mjs` | Ocean 이 작성한 진입 코드와 빌드 스크립트 |
| `package.json`, `package-lock.json` | 버전 고정 (빌드 도구 esbuild 는 배포물에 들어가지 않는다) |
| `LICENSE` | Apache License 2.0 전문 (MathJax, mhchemParser) |
| `LICENSE.fonts-OFL.txt` | 번들에 path 데이터로 들어가는 MathJax TeX 글꼴의 OFL-1.1 고지 |
| `LICENSE.esbuild` | 번들에 들어가는 esbuild 보조 함수(MIT, (c) 2020 Evan Wallace) |

번들에 들어가는 패키지 (build.mjs 가 다른 패키지가 섞이면 빌드를 실패시킨다):

| 패키지 | 버전 | 라이선스 | 저작권 |
| --- | --- | --- | --- |
| mathjax-full | 3.2.2 | Apache-2.0 | (c) 2009-2022 The MathJax Consortium; color 패키지 (c) 2018-2022 Omar Al-Ithawi and The MathJax Consortium |
| mhchemparser | 4.2.1 | Apache-2.0 | (c) 2015-2023 Martin Hensel |
| MathJax TeX 글꼴 데이터 | (mathjax-full 내장) | OFL-1.1 | (c) 2009-2010 Design Science, Inc. — Computer Modern (Knuth), AMSFonts (AMS, OFL), RSFS (Ralph Smith) 에서 생성 |

- Apache-2.0 패키지들에는 NOTICE 파일이 없다(npm 패키지와 GitHub 저장소 확인). 고지는 `THIRD_PARTY_NOTICES.md` 에 있다.
- 쓰지 않는 TeX 확장: `autoload`, `require`(실행 중 파일을 읽으려 함), `noerrors`, `noundefined`(오류를 감춤).
- 번들에 들어가지 않는 것: speech-rule-engine, xmldom, mj-context-menu 등 mathjax-full 의 다른 의존성.
- 상표: MathJax 로고를 쓰거나 MathJax 가 보증/제휴한 것처럼 표현하지 않는다. "MathJax 를 사용한다"는 사실 표기만 한다.

### Ocean 쪽 처리 (`src/UI/Common/Control/MathRenderer.cpp`)

- SVG 의 `currentColor` 를 글자색으로 바꾼다.
- `stroke-width="0"` 을 아주 얇은 값으로 바꾼다. Qt 는 굵기 0 을 1px 선으로 그려 글자가 굵어진다.
- 한글처럼 MathJax 글꼴에 없는 글자는 `<text font-family="serif">` 로 나오므로 본문 글꼴로 바꾸고,
  글자 폭(`setCjkCharWidth`)을 본문 글꼴의 실제 폭으로 맞춘다(첫 변환 전에 한 번).

### 왜 MathJax v3 인가 (2026-10 검토)

- v4(@mathjax/src 4.1.3)의 기본 글꼴(mathjax-newcm)은 GUST/LPPL 데이터이고, 원본 New Computer Modern 8.0 부터 GPL 로 바뀌었다.
  v4 + mathjax-tex 글꼴은 라이선스상 v3 와 같지만, Qt 5.15 의 QJSEngine 에서 실행되지 않는다(ES2022 함수 부재, 엔진의 변수 범위 버그).
  Qt 6 으로 올릴 때 v4 + `@mathjax/mathjax-tex-font` 를 다시 검토한다(newcm 은 쓰지 않는다).
- 이전에 쓰던 MicroTeX 는 JLaTeXMath(GPL-2.0+)를 옮긴 파생물로 보여 제거했다.

### 다시 만들기 (Node.js 필요, 번들을 바꿀 때만)

```bash
cd resources/mathjax
npm ci            # package-lock.json 기준으로 설치
npm run build     # mathjax-bundle.js 생성 (번들에 든 패키지 목록을 출력)
rm -rf node_modules
```

버전을 올릴 때는 바뀐 패키지와 글꼴 데이터의 라이선스를 다시 감사한다.

## Cascadia Mono (저장소에 남긴 글꼴)

- 원본: https://github.com/microsoft/cascadia-code 릴리스 `v2407.24` 의 `CascadiaCode-2407.24.zip` 안 `ttf/static/CascadiaMono-{Regular,Bold,Italic,BoldItalic}.ttf`
  (SHA256 은 위 터미널 GPU 절, Regular `06520d032ec274fa5040b22c6f4a1d829081b24ba40b2da56dae89bf10c7b481`, 글꼴 버전 2407.024). 공식 배포가 150MB zip 뿐이라 내려받지 않고 저장소에 둔다.
- 라이선스: SIL OFL 1.1 (`resources/fonts/OFL.txt`). 원본 그대로 번들한다(수정·이름 변경 없음).
- 미리보기 코드 블록 글꼴. `resources/fonts/cascadia_mono.qrc` 로 앱에 넣고 `BundledFonts::Register()`(`src/Core/BundledFonts.cpp`, `Application` 이 부름)가 `:/fonts/CascadiaMono-{Regular,Bold,Italic,BoldItalic}.ttf` 를 한 번만 등록한다. 굵은/기울임 글꼴을 따로 두어 합성 굵기를 쓰지 않는다.

## Editorial 시안 서체 (저장소에 남긴 글꼴, 2026-10-24 감사)

Editorial 시안([계획](superpowers/plans/2026-10-24-editorial-skin.md))의 서체 역할: UI = Inter, 타이틀 = Inter Display, 코드 · 번호 = IBM Plex Mono, 제목 · 숫자 · 본문 = Noto Serif KR.
`resources/fonts/editorial_fonts.qrc` 로 앱에 넣고 `BundledFonts::Register()` 가 `:/fonts/` 아래 여덟 파일을 한 번만 등록한다. 시안이 고르는 글꼴은 `ISkin::GetFonts()` 후보 중 설치된 첫 글꼴이다(`Theme::ResolveFontFamilies`).
rcc 는 70% 이상 줄 때만 압축해서 글꼴이 그대로 들어가므로 이 qrc 는 파일마다 `compress="9" threshold="0"` 로 압축한다(글꼴 9.32MB → 실행 파일 +6.1MB, 시작할 때 풀기 약 30ms).
라이선스 전문은 `resources/fonts/OFL-{Inter,IBMPlexMono,NotoSerifKR}.txt` 이고 설치본에 `licenses/LICENSE.{Inter,IBMPlexMono,NotoSerifKR}-OFL` 로 들어간다(`CMakeLists.txt`). 앱 안 고지는 `THIRD_PARTY_NOTICES.md`.

| 파일 (`resources/fonts/`) | 크기 | SHA256 | 원본 | 글꼴 버전 (name 5) |
| --- | --- | --- | --- | --- |
| `Inter-Regular.ttf` | 411,640 | `40d692fce188e4471e2b3cba937be967878f631ad3ebbbdcd587687c7ebe0c82` | Inter v4.1 `extras/ttf/Inter-Regular.ttf` (그대로) | 4.001;git-9221beed3 |
| `Inter-Medium.ttf` | 417,300 | `97ad806f526e41546d46365bb3a393145f75b7b1568913db74549ad8b8dba872` | Inter v4.1 `extras/ttf/Inter-Medium.ttf` (그대로) | 4.001;git-9221beed3 |
| `Inter-SemiBold.ttf` | 419,744 | `78a843fade9d4612a5567302fb595b56976eb5fcebf4fea5a5912d638bafcde3` | Inter v4.1 `extras/ttf/Inter-SemiBold.ttf` (그대로) | 4.001;git-9221beed3 |
| `InterDisplay-SemiBold.ttf` | 418,540 | `0310d7a325896129730c6c8cf9a6e0f81ee258bedf77b1ff059b2a7b75f74e02` | Inter v4.1 `extras/ttf/InterDisplay-SemiBold.ttf` (그대로) | 4.001;git-9221beed3 |
| `IBMPlexMono-Regular.ttf` | 173,052 | `7c6fbddca4b700be918f5f6183d9bd4464fa427fe435f0b480d77fe2bb8c5a43` | Plex Mono 2.5.0 `ibm-plex-mono/fonts/complete/ttf/IBMPlexMono-Regular.ttf` (그대로) | 2.005 |
| `IBMPlexMono-Medium.ttf` | 174,008 | `98fbd727aae340b236955879dabed4d991aac9e8e90b3b2a67ce4a59221cc97c` | Plex Mono 2.5.0 `ibm-plex-mono/fonts/complete/ttf/IBMPlexMono-Medium.ttf` (그대로) | 2.005 |
| `NotoSerifKR-Regular.otf` | 3,709,092 | `2cf6ea64714e7571587873ac2aca0b19c869decb649ba76e44358e514bb50a0e` | Serif2.003 `SubsetOTF/KR/NotoSerifKR-Regular.otf` 의 서브셋 | 2.003;hotconv 1.1.1;makeotfexe 2.6.0 |
| `NotoSerifKR-SemiBold.otf` | 3,596,848 | `7fb91a87c9d2110b8bf7970b1915c48a75efa405f7d3720c6d5fdfb2ffdddf82` | Serif2.003 `SubsetOTF/KR/NotoSerifKR-SemiBold.otf` 의 서브셋 | 2.003;hotconv 1.1.1;makeotfexe 2.6.0 |

여덟 파일 모두 OS/2 `fsType` 0(설치 가능 임베딩)이다. Qt 는 typographic · WWS 이름(name 16/17, 21/22)으로 묶어 가족 "Inter"(Regular · Medium · SemiBold), "Inter Display"(SemiBold), "IBM Plex Mono"(Regular · Medium), "Noto Serif KR"(Regular · SemiBold)로 본다(FreeType · Windows GDI 글꼴 데이터베이스에서 확인. Noto Serif KR 의 Windows 11 은 아래 절).

### Inter 4.1

- 원본: https://github.com/rsms/inter/releases/download/v4.1/Inter-4.1.zip (릴리스 `v4.1`, 2024-11-16), 33,707,794 bytes, SHA256 `9883fdd4a49d4fb66bd8177ba6625ef9a64aa45899767dde3d36aa425756b11e`. `extras/ttf/` 는 TrueType 힌트가 든 정적 글꼴이다.
- 라이선스: zip 의 `LICENSE.txt` 를 그대로 `OFL-Inter.txt` 로 둔다(SHA256 `262481e844521b326f5ecd053e59b98c8b2da78c8ee1bdbb6e8174305e54935a`). "Copyright (c) 2016 The Inter Project Authors (https://github.com/rsms/inter)" + SIL OFL 1.1 전문. **Reserved Font Name 없음.**
- 이름 테이블: 저작권(name 0) "Copyright 2016 The Inter Project Authors", 상표(7) "Inter UI and Inter is a trademark of rsms.", 라이선스(13 · 14) OFL 1.1 · `http://scripts.sil.org/OFL`.
- 수정: 없음.

### IBM Plex Mono 2.5.0

- 원본: https://github.com/IBM/plex/releases/download/%40ibm/plex-mono%402.5.0/ibm-plex-mono.zip (릴리스 `@ibm/plex-mono@2.5.0`, 2026-06-11), 6,940,652 bytes, SHA256 `6d23f01257663d8cc49a0d64c22ced630b79e0e2a0ac08a0da86e9a38bbc481c`.
- 라이선스: 글꼴 옆의 `ibm-plex-mono/fonts/complete/ttf/license.txt` 를 그대로 `OFL-IBMPlexMono.txt` 로 둔다(SHA256 `91c25c350d3cac39da2736d74f7ba37ef648f5237a4e330a240615bc8d8c4360`. 묶음 맨 위 `LICENSE.txt` 와 글은 같고 줄바꿈만 다르다).
  "Copyright © 2017 IBM Corp. with Reserved Font Name "Plex"" + SIL OFL 1.1 전문. **RFN "Plex"**: 원본 그대로 넣으므로 문제없다. 고친 판(서브셋 등)을 만들면 이름에 Plex 를 쓸 수 없다.
- 이름 테이블: 저작권 "Copyright 2017 IBM Corp. All rights reserved.", 상표 "IBM Plex® is a trademark of IBM Corp, registered in many jurisdictions worldwide.", 라이선스 OFL 1.1 · `http://scripts.sil.org/OFL`. Medium 의 name 1 은 "IBM Plex Mono Medm"(name 16/17 "IBM Plex Mono"/"Medium").
- 수정: 없음.

### Noto Serif KR 2.003 (서브셋)

- 원본: https://github.com/notofonts/noto-cjk/releases/download/Serif2.003/13_NotoSerifKR.zip (릴리스 `Serif2.003`, 2024-07-30, 지역 서브셋 OTF 묶음), 39,751,410 bytes, SHA256 `ac3398873346bc97e00a9652afe791cad6a2df3e5f157c4e7d40ce60b2b88605`.
  쓴 파일(CFF, 글리프 24,910, cmap 23,124자): `SubsetOTF/KR/NotoSerifKR-Regular.otf` 7,669,020 bytes `5ea012e15cb7eacc1f680aee1703f3b164791b1443ea3e52b65080cca5d179cf`, `SubsetOTF/KR/NotoSerifKR-SemiBold.otf` 7,610,748 bytes `a63051d299d7219132a98ff3c2b76a62022344e50e9f3b50fdb9b0ee4560f583`.
- 라이선스: zip 의 `LICENSE` 를 그대로 `OFL-NotoSerifKR.txt` 로 둔다(SHA256 `6a73f9541c2de74158c0e7cf6b0a58ef774f5a780bf191f2d7ec9cc53efe2bf2`, 저장소 태그 `Serif2.003` 의 `Serif/LICENSE` 와 같은 바이트. 저장소 맨 위에는 LICENSE 가 없다).
  SIL OFL 1.1 전문만 있고 저작권 줄 · RFN 줄이 없다. 저작권은 이름 테이블(name 0) "© 2017-2024 Adobe (http://www.adobe.com/)." 이고 여기에도 RFN 이 없다. **Reserved Font Name 없음**: 서브셋(OFL 의 Modified Version)이 "Noto Serif KR" 이름을 그대로 써도 OFL §3 에 걸리지 않는다. Adobe 원본(Source Han Serif)의 RFN "Source" 는 쓰지 않는다.
- 상표: name 7 "Noto is a trademark of Google Inc." OFL 은 상표권을 주지 않는다. 앱은 Noto 를 제품 이름으로 쓰지 않고 글꼴 가족 이름으로만 쓰며, 고지에 서브셋이라고 밝힌다.
- OFL §2(저작권 고지 · 라이선스 동봉): 이름 테이블 레코드(저작권 0, 상표 7, 라이선스 13 · 14, 한국어 이름 포함 18 · 22개)를 원본과 똑같이 두고, 전문을 설치본에 넣는다.
- 서브셋(Ocean 수정, 2026-10-24): fontTools 4.67.0 `pyftsubset`, 두 굵기에 같은 명령. 같은 명령을 두 번 돌려 같은 해시가 나온다.

```bash
pyftsubset NotoSerifKR-Regular.otf \
  --unicodes="U+0000-00FF,U+0100-017F,U+0300-036F,U+0370-03FF,U+1100-11FF,U+2000-206F,U+2070-209F,U+20A0-20CF,U+2100-214F,U+2150-218F,U+2190-21FF,U+2200-22FF,U+2460-24FF,U+25A0-25FF,U+2600-26FF,U+2700-27BF,U+3000-303F,U+3130-318F,U+AC00-D7A3,U+FF00-FFEF" \
  --layout-features="ccmp,locl,mark,mkmk,kern,liga,clig,calt,rlig,rclt" \
  --name-IDs='*' --name-languages='*' --name-legacy \
  --notdef-glyph --notdef-outline \
  --output-file=NotoSerifKR-Regular.otf          # SemiBold 도 같다
```

  - 남긴 글자(원본에 있는 것만, cmap 12,550자 · 글리프 13,129): 한글 음절 11,172자 전부, 첫가끝 자모 256, 호환 자모 94, 라틴(기본 · Latin-1 191, 확장 A 30), 결합 부호 5, 그리스 49, 일반 문장 부호, 위첨자 · 아래첨자, 통화 기호, 글자꼴 기호, 숫자꼴, 화살표, 수학 연산자, 원 숫자, 도형, 기호, 딩뱃(스펙의 → ← ↑ · × √ ✓ … 포함), CJK 기호 · 문장 부호, 전각 · 반각 꼴.
  - 뺀 글자: 한자(CJK 통합 8,138 · 호환 332), 키릴, 가나 등. 없는 글자는 Qt 가 다른 글꼴로 그린다.
  - OpenType 기능: HarfBuzz 가 가로쓰기에 기본으로 켜는 것만 남겼다(결과에 ccmp, locl, liga, calt, kern 이 남는다). 세로쓰기(vert, vrt2, vhal, vkrn, vpal), 폭 바꾸기(fwid, hwid, pwid, halt, palt), aalt, dlig, ruby, hist, 옛한글 조합(ljmo, vjmo, tjmo)은 뺐다. Qt 5.15 는 이 기능들을 켜지 않아 그리는 모양은 같고, 옛한글 첫가끝 조합만 낱자로 보인다(현대 한글 첫가끝은 HarfBuzz 가 완성형으로 합친다).
  - fontTools 가 지운 표: DSIG(고친 글꼴에는 서명이 맞지 않는다). 글리프 윤곽 · 힌트 · 메트릭 · 이름 테이블 · 글꼴 버전은 그대로다.
  - 크기: 원본 둘이 15.3MB → 7.3MB. 힌트까지 빼면(`--no-hinting`) 둘이 약 5.8MB 가 되지만 13.5px 본문의 Windows 렌더링을 위해 남겼다.
- Windows 11 에는 같은 가족 이름의 시스템 글꼴 `NotoSerifKR-VF.ttf`(가변 글꼴)가 있다. Qt 의 Windows(GDI) 글꼴 데이터베이스는 그때 시스템 글꼴로 그린다(2026-10-24 확인: 앱 글꼴을 등록해도 한자가 든 시스템 글꼴이 잡힌다). 같은 서체라 모양은 같다. 앱에 넣은 서브셋은 이 가족이 없는 시스템(Windows 10, macOS, 대부분의 Linux)과 offscreen 테스트에서 쓰인다.
- 버전을 올릴 때: 새 릴리스의 `LICENSE` 와 이름 테이블에서 저작권 · RFN 을 다시 확인하고(RFN 이 생기면 서브셋은 이름을 바꿔야 한다), 위 명령으로 다시 만들어 이 절의 해시를 고친다.
