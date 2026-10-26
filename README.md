# DentalLabeler

구강 스캔 메시(PLY)에서 정점마다 클래스(치은 · 치아 · 스캔바디)와 치아 번호(FDI)를 붙이는 세그멘테이션 라벨링 데스크톱 도구다.
화면 기획은 [docs/ScreenPlan.html](docs/ScreenPlan.html) 에 있다.

## 지금 되는 것 (M1: 라벨링 작업 화면 DL-20 의 P0)

- PLY 열기 (ascii · binary, 메시와 포인트 클라우드). 파일 이름의 `lower` · `upper` 로 턱을 정한다
- 3D 보기: 셰이딩 · 라벨 표시, 라벨 불투명도, 교합면 · 정면 · 좌 · 우 · 설측 시점 (치열 방향은 메시 모양에서 추정)
- 도구: 이동 `V` · 브러시 `B` · 올가미 `L` · 채우기 `F` · 지우개 `E`
  - 브러시는 표면을 따라 잰 반경(mm)으로 칠하고, '곡률 경계에서 멈춤'을 켜면 치아와 치은 사이 고랑을 넘지 않는다
  - 덮어쓰기 규칙(모든 라벨 · 미분류와 치은만 · 미분류만), 라벨 잠금, '보이는 면만'(올가미는 가림 검사까지)
- 라벨 고르기: 숫자 두 개(`3` `6` → 36), `0`·`G` 치은, `Tab` 아치 순서 다음 치아, `Alt`+클릭 스포이드, 치식도 클릭
- 클래스: `Alt+0` 치은 · `Alt+1` 치아 · `Alt+2` 스캔바디 (오른쪽 패널 버튼도 같다). 숫자 두 개는 지금 클래스의 자리를 고른다 (`Alt+2` → `3` `6` = 36 자리 스캔바디)
- 치아 상태(결손 · 임플란트 · 보철 · 부분 맹출 · 과잉치)와 메모
- 실행 취소 · 다시 실행(스트로크 단위, 기록 탭에서 원하는 시점으로 이동)
- 저장: 원본 `<이름>.ply` 는 건드리지 않고, 원본 정점 속성을 그대로 복사한 `<이름>.labeled.ply` 에 정점마다 int 필드 세 개를 쓴다
  - 치아 상태 · 메모 · 작업 상태와 클래스 목록 · instance 목록은 헤더의 `comment dentallabeler …` 줄에 쓴다
  - 열 때 원본과 정점 수 · 위치가 다르면 열지 않는다. 원본 없이 `.labeled.ply` 만 열면 그 파일로 작업한다
  - `scalar_fdi` · `scalar_label` · `scalar_instance`(CloudCompare 이름)와 이전 형식(`label` = FDI 번호)도 읽는다
  - 다른 도구가 원본에 넣어 둔 라벨 필드가 규칙에 맞으면 그것으로 시작한다
- 2분마다 `<이름>.autosave.ply` 에 자동 저장하고, 비정상 종료 뒤 다시 열면 복구를 묻는다

| 필드 | 뜻 | 값 |
| --- | --- | --- |
| `fdi` | 치아 번호 (치아 · 스캔바디가 놓인 자리) | 11–48, 자리가 없으면 0 |
| `label` | 클래스 | −1 미분류, 0 치은, 1 치아, 2 스캔바디 (`src/Core/LabelTable.cpp` 의 클래스 표에 더할 수 있다) |
| `instance` | 대상 번호 | 치은 · 미분류 0. 칠한 대상(클래스 + 자리)마다 1부터, 클래스 순서 → 치식 순서(상악 18 … 28, 하악 48 … 38). 저장할 때마다 다시 매긴다 |

Python 에서 읽기 (plyfile 예):

```python
from plyfile import PlyData
v = PlyData.read("case_0001_lower.labeled.ply")["vertex"]
fdi, label, instance = v["fdi"], v["label"], v["instance"]   # int32, 정점 순서는 원본과 같다
```

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
