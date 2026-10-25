# DentalLabeler

구강 스캔 메시(PLY)에서 정점마다 치아 번호(FDI)나 치은 라벨을 붙이는 세그멘테이션 라벨링 데스크톱 도구다.
화면 기획은 [docs/ScreenPlan.html](docs/ScreenPlan.html) 에 있다.

## 지금 되는 것 (M1: 라벨링 작업 화면 DL-20 의 P0)

- PLY 열기 (ascii · binary, 메시와 포인트 클라우드). 파일 이름의 `lower` · `upper` 로 턱을 정한다
- 3D 보기: 셰이딩 · 라벨 표시, 라벨 불투명도, 교합면 · 정면 · 좌 · 우 · 설측 시점 (치열 방향은 메시 모양에서 추정)
- 도구: 이동 `V` · 브러시 `B` · 올가미 `L` · 채우기 `F` · 지우개 `E`
  - 브러시는 표면을 따라 잰 반경(mm)으로 칠하고, '곡률 경계에서 멈춤'을 켜면 치아와 치은 사이 고랑을 넘지 않는다
  - 덮어쓰기 규칙(모든 라벨 · 미분류와 치은만 · 미분류만), 라벨 잠금, '보이는 면만'(올가미는 가림 검사까지)
- 라벨 고르기: 숫자 두 개(`3` `6` → 36), `0`·`G` 치은, `Tab` 아치 순서 다음 치아, `Alt`+클릭 스포이드, 치식도 클릭
- 치아 상태(결손 · 임플란트 · 보철 · 부분 맹출 · 과잉치)와 메모
- 실행 취소 · 다시 실행(스트로크 단위, 기록 탭에서 원하는 시점으로 이동)
- 저장: 메시 옆 `<이름>.labels.json`. 원본의 정점 수와 SHA-256 을 기억해 원본이 바뀌면 열지 않는다
- 2분마다 `<이름>.autosave` 에 자동 저장하고, 비정상 종료 뒤 다시 열면 복구를 묻는다

전체 단축키는 앱의 도움말 › 단축키(`?`)에 있다.

## 아직 안 되는 것

기획서의 케이스 목록(DL-10) · AI 예측 검토(DL-22) · 품질 검사(DL-23) · 검수(DL-24) · 가져오기와 내보내기(DL-30 · 31) · 라벨 체계(DL-40) · 설정(DL-50), STL · OBJ · PCD 읽기, 영역 확장 · 마진 라인 · 경계 다듬기 · 클리핑 도구.
숨긴 라벨의 면 뒤쪽은 아직 커서로 집을 수 없다.

## 빌드

필요한 것: Qt 5.15.2 (MinGW 8.1 64-bit, `C:\Qt\5.15.2\mingw81_64`), Qt 에 딸린 MinGW 8.1 (`C:\Qt\Tools\mingw810_64`), CMake 3.21 이상, Ninja.
경로가 다르면 `CMakeUserPresets.json` 에서 `CMAKE_PREFIX_PATH` · `CMAKE_CXX_COMPILER` · `PATH` 를 바꾼 프리셋을 만든다.

```powershell
cmake --preset mingw-release
cmake --build --preset mingw-release
ctest --preset mingw-release
```

UI 없이 Core 와 테스트만 빌드하려면 `-DDL_BUILD_APP=OFF` 를 준다.

## 실행

Qt DLL 이 PATH 에 있어야 한다. 탐색기에서 바로 실행하려면 먼저 `cmake --build --preset mingw-release --target deploy` 로 DLL 을 실행 파일 옆에 복사한다.

```powershell
$env:PATH = "C:\Qt\Tools\mingw810_64\bin;C:\Qt\5.15.2\mingw81_64\bin;$env:PATH"
.\build\mingw-release\DentalLabeler.exe data\case_0001_lower.ply
```

화면 확인용 옵션: `--screenshot <png>` 는 케이스가 열리면 창을 그림으로 저장하고 끝낸다. `--demo-stroke` 를 함께 주면 저장 전에 브러시 획을 몇 번 긋는다(라벨 파일은 쓰지 않는다).

## 구조

`docs/Qt.md` 의 구조를 따른다. 의존은 UI → Presenter → 비즈니스 한 방향이다.

```
src/
├─ Core/          Application(조립), Observers, SystemStatus, Types, LabelTable(FDI 이름 · 색)
├─ Interface/     IPaintStrategy
├─ Algorithm/
│  ├─ Geometry/   MeshTopology, Bvh, Curvature, DentalFrame
│  └─ LabelPaint/ LabelPaint + Strategy/(BrushPaintCPU, FillPaintCPU, LassoPaintCPU)
├─ Document/      Mesh, PlyReader, LabelDocument, LabelFile, EditHistory, CaseData
├─ Thread/        MeshLoadThread (읽기 · 법선 · 인접 그래프 · 곡률 · BVH)
├─ Presenter/     Workspace/WorkspacePresenter
└─ UI/
   ├─ Common/     OrbitCamera, LabelKeyInput, Theme/IconFactory, Control/OdontogramWidget
   ├─ View/       Workspace/(Viewport3DWidget, ToolRailView, ToolOptionsView, LabelPanelView)
   └─ Skin/       Default/(MainWindow.ui, Default.qss)
tests/            Qt Test (PLY 읽기, 기하, 칠하기 전략, 라벨 문서, Presenter 통합)
```

새 서드파티 라이브러리는 쓰지 않았다(Qt 의 Core · Gui · Widgets · Concurrent · Svg 만 동적 링크). 들일 때는 [docs/LicenseCompliance.md](docs/LicenseCompliance.md) 의 절차를 따른다.
